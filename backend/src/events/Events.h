// ============================================================================
// File: backend/src/events/Events.h
// Version: 4.2.1
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Event structures for EventBus system
//   Header-only file defining all system events
//
// ============================================================================

#pragma once

#include "../midi/MidiMessage.h"
#include <string>
#include <cstdint>

namespace midiMind {
namespace events {

// ============================================================================
// MIDI DEVICE EVENTS
// ============================================================================

/**
 * @struct MidiMessageReceivedEvent
 * @brief Event published when a MIDI message is received from a device
 * 
 * This event is triggered by device callbacks configured via devices.setCallback
 * and published through EventBus to all subscribers (including ApiServer for
 * WebSocket broadcasting).
 */
struct MidiMessageReceivedEvent {
    std::string deviceId;      // Device identifier
    std::string deviceName;    // Device name for display
    MidiMessage message;       // The MIDI message received
    uint64_t timestamp;        // Timestamp (nanoseconds since epoch)
    
    MidiMessageReceivedEvent(const std::string& id, 
                            const std::string& name,
                            const MidiMessage& msg,
                            uint64_t ts)
        : deviceId(id)
        , deviceName(name)
        , message(msg)
        , timestamp(ts)
    {}
};

/**
 * @struct MidiMessageSentEvent
 * @brief Event published when a MIDI message is sent to a device
 */
struct MidiMessageSentEvent {
    std::string deviceId;
    std::string deviceName;
    MidiMessage message;
    uint64_t timestamp;
    bool success;
    
    MidiMessageSentEvent(const std::string& id,
                        const std::string& name,
                        const MidiMessage& msg,
                        uint64_t ts,
                        bool s)
        : deviceId(id)
        , deviceName(name)
        , message(msg)
        , timestamp(ts)
        , success(s)
    {}
};

/**
 * @struct DeviceConnectedEvent
 * @brief Event published when a device connects
 */
struct DeviceConnectedEvent {
    std::string deviceId;
    std::string deviceName;
    std::string deviceType;
    uint64_t timestamp;
    
    DeviceConnectedEvent(const std::string& id,
                        const std::string& name,
                        const std::string& type,
                        uint64_t ts)
        : deviceId(id)
        , deviceName(name)
        , deviceType(type)
        , timestamp(ts)
    {}
};

/**
 * @struct DeviceDisconnectedEvent
 * @brief Event published when a device disconnects
 */
struct DeviceDisconnectedEvent {
    std::string deviceId;
    std::string deviceName;
    std::string reason;
    uint64_t timestamp;
    
    DeviceDisconnectedEvent(const std::string& id,
                           const std::string& name,
                           const std::string& r,
                           uint64_t ts)
        : deviceId(id)
        , deviceName(name)
        , reason(r)
        , timestamp(ts)
    {}
};

// ============================================================================
// PLAYBACK EVENTS
// ============================================================================

/**
 * @struct PlaybackStateChangedEvent
 * @brief Event published when playback state changes
 */
struct PlaybackStateChangedEvent {
    enum class State {
        STOPPED,
        PLAYING,
        PAUSED
    };
    
    State state;
    std::string filepath;
    double position;
    uint64_t timestamp;
    
    PlaybackStateChangedEvent(State s, 
                             const std::string& f,
                             double p,
                             uint64_t ts)
        : state(s)
        , filepath(f)
        , position(p)
        , timestamp(ts)
    {}
};

/**
 * @struct PlaybackProgressEvent
 * @brief Event published periodically during playback
 */
struct PlaybackProgressEvent {
    double position;      // Current position in milliseconds
    double duration;      // Total duration in milliseconds
    double percentage;    // Progress percentage (0-100)
    uint64_t timestamp;
    
    PlaybackProgressEvent(double pos, double dur, double pct, uint64_t ts)
        : position(pos)
        , duration(dur)
        , percentage(pct)
        , timestamp(ts)
    {}
};

// ============================================================================
// ROUTING EVENTS
// ============================================================================

/**
 * @struct RouteAddedEvent
 * @brief Event published when a route is added
 */
struct RouteAddedEvent {
    std::string source;
    std::string destination;
    uint64_t timestamp;
    
    RouteAddedEvent(const std::string& src,
                   const std::string& dst,
                   uint64_t ts)
        : source(src)
        , destination(dst)
        , timestamp(ts)
    {}
};

/**
 * @struct RouteRemovedEvent
 * @brief Event published when a route is removed
 */
struct RouteRemovedEvent {
    std::string source;
    std::string destination;
    uint64_t timestamp;
    
    RouteRemovedEvent(const std::string& src,
                     const std::string& dst,
                     uint64_t ts)
        : source(src)
        , destination(dst)
        , timestamp(ts)
    {}
};

// ============================================================================
// SYSTEM EVENTS
// ============================================================================

/**
 * @struct SystemErrorEvent
 * @brief Event published when a system error occurs
 */
struct SystemErrorEvent {
    std::string component;
    std::string message;
    std::string errorCode;
    int severity;  // 0=info, 1=warning, 2=error, 3=critical
    uint64_t timestamp;
    
    SystemErrorEvent(const std::string& comp,
                    const std::string& msg,
                    const std::string& code,
                    int sev,
                    uint64_t ts)
        : component(comp)
        , message(msg)
        , errorCode(code)
        , severity(sev)
        , timestamp(ts)
    {}
};

} // namespace events
} // namespace midiMind

// ============================================================================
// END OF FILE Events.h
// ============================================================================
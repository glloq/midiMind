// ============================================================================
// File: backend/src/core/EventBus.cpp
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Implementation file for EventBus. Minimal implementation as most
//   functionality is template-based in the header file.
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Simplified to minimal implementation
//   - Removed async threading (kept synchronous for v4.1.0)
//   - Added helper functions
//   - Improved logging
//
// ============================================================================

#include "EventBus.h"
#include "Logger.h"

namespace midiMind {

// ============================================================================
// SUBSCRIPTION IMPLEMENTATION
// ============================================================================

// Note: Subscription class is fully implemented in header (inline)
// No additional implementation needed here

// ============================================================================
// EVENTBUS - HELPER FUNCTIONS
// ============================================================================

// Note: EventBus is primarily template-based and implemented in header
// This file provides any non-template utility functions if needed

/**
 * @brief Helper function to format event statistics
 * @param bus EventBus instance
 * @return Formatted statistics string
 */
std::string formatEventBusStatistics(const EventBus& bus) {
    std::ostringstream oss;
    
    oss << "EventBus Statistics:\n";
    oss << "  Event types: " << bus.getEventTypeCount() << "\n";
    oss << "  Total events published: " << bus.getTotalEventsPublished() << "\n";
    
    return oss.str();
}

} // namespace midiMind

// ============================================================================
// END OF FILE EventBus.cpp v4.1.0
// ============================================================================
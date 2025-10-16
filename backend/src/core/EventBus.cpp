// ============================================================================
// File: backend/src/core/EventBus.cpp
// Version: 4.1.0 - CORRIGÉ
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Implementation of EventBus utility functions.
//   Main class methods are inline in header.
//
// Author: MidiMind Team
// Date: 2025-10-17
//
// Changes v4.1.0:
//   - Fixed method calls (getEventTypeCount, getTotalEventsPublished)
//   - All functions now correctly reference EventBus methods
//
// ============================================================================

#include "EventBus.h"
#include <sstream>
#include <iomanip>

namespace midiMind {

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Format EventBus statistics as string
 * @param bus EventBus instance
 * @return std::string Formatted statistics
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
// END OF FILE EventBus.cpp
// ============================================================================
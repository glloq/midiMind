// ============================================================================
// File: backend/src/core/EventBus.cpp
// Version: 4.2.0 - CORRECTIONS CRITIQUES
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Changes v4.2.0:
//   🔧 ADDED: logError implementation with Logger
//   ✅ FIXED: Proper error logging in event handlers
//
// ============================================================================

#include "EventBus.h"
#include "Logger.h"
#include <sstream>
#include <iomanip>

namespace midiMind {

// ============================================================================
// EVENTBUS IMPLEMENTATION
// ============================================================================

void EventBus::logError(const char* component, const std::string& message) {
    Logger::error(component, message);
}

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
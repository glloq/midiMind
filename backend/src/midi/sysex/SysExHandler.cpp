// ============================================================================
// File: backend/src/midi/sysex/SysExHandler.cpp
// Version: 4.1.1
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Implementation of SysExHandler - processes standard and custom SysEx
//   messages. Focuses on Blocks 1-2 for v4.1.0.
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.1:
//   - Fixed std::min type warnings
//   - Used atomic operations for configuration
//
// ============================================================================

#include "SysExHandler.h"
#include "SysExParser.h"
#include "../../core/Logger.h"

#include <algorithm>
#include <sstream>
#include <iomanip>

namespace midiMind {

// ============================================================================
// CONSTRUCTION / DESTRUCTION
// ============================================================================

SysExHandler::SysExHandler()
    : autoIdentify_(true)
    , autoIdentifyDelayMs_(500)
{
    Logger::info("SysExHandler", "SysExHandler initialized");
}

SysExHandler::~SysExHandler() {
    Logger::info("SysExHandler", "SysExHandler destroyed");
}

// ============================================================================
// MESSAGE HANDLING
// ============================================================================

void SysExHandler::handleSysExMessage(const std::vector<uint8_t>& data,
                                      const std::string& deviceId) {
    // Validate basic SysEx format
    if (data.size() < 4) {
        Logger::warning("SysExHandler", 
            "Invalid SysEx from " + deviceId + " (too short)");
        return;
    }
    
    if (data[0] != 0xF0 || data[data.size() - 1] != 0xF7) {
        Logger::warning("SysExHandler",
            "Invalid SysEx from " + deviceId + " (missing F0/F7)");
        return;
    }
    
    messagesReceived_++;
    
    // Log received message
    std::stringstream ss;
    ss << "Received SysEx from " << deviceId << " (" << data.size() << " bytes): ";
    size_t logLimit = std::min(data.size(), static_cast<size_t>(16));
    for (size_t i = 0; i < logLimit; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') 
           << static_cast<int>(data[i]) << " ";
    }
    if (data.size() > 16) ss << "...";
    Logger::debug("SysExHandler", ss.str());
    
    // Dispatch based on message type
    if (SysExParser::isIdentityReply(data)) {
        handleIdentityReply(data, deviceId);
    } else if (SysExParser::isCustomSysEx(data)) {
        auto blockId = SysExParser::getCustomBlockId(data);
        
        if (!blockId) {
            Logger::warning("SysExHandler", 
                "Invalid Custom SysEx from " + deviceId);
            return;
        }
        
        switch (*blockId) {
            case 1: // Block 1: Identification
                handleCustomIdentification(data, deviceId);
                break;
                
            case 2: // Block 2: Note Map
                handleNoteMap(data, deviceId);
                break;
                
            default:
                Logger::debug("SysExHandler",
                    "Custom SysEx Block " + std::to_string(*blockId) + 
                    " not implemented yet");
                break;
        }
    } else {
        Logger::debug("SysExHandler", 
            "Unhandled SysEx type from " + deviceId);
    }
}

// ============================================================================
// INTERNAL HANDLERS
// ============================================================================

void SysExHandler::handleIdentityReply(const std::vector<uint8_t>& data,
                                       const std::string& deviceId) {
    Logger::info("SysExHandler", "Standard Identity Reply from " + deviceId);
    
    // Parse without lock
    auto identity = SysExParser::parseIdentityReply(data);
    if (!identity) {
        Logger::warning("SysExHandler", "Failed to parse Identity Reply");
        return;
    }
    
    // Store in cache with short lock
    {
        std::lock_guard<std::mutex> lock(mutex_);
        identityCache_[deviceId] = *identity;
        identityRepliesReceived_++;
    }
    
    // Log details
    std::stringstream ss;
    ss << "Device identified: "
       << "Manufacturer=0x" << std::hex << identity->manufacturerId
       << ", Family=0x" << identity->familyCode
       << ", Model=0x" << identity->modelNumber
       << ", Version=" << std::dec 
       << static_cast<int>(identity->versionMajor) << "."
       << static_cast<int>(identity->versionMinor) << "."
       << static_cast<int>(identity->versionPatch);
    Logger::info("SysExHandler", ss.str());
    
    // Callback without lock
    if (onDeviceIdentified_) {
        onDeviceIdentified_(deviceId, *identity);
    }
}

void SysExHandler::handleCustomIdentification(const std::vector<uint8_t>& data,
                                              const std::string& deviceId) {
    Logger::info("SysExHandler", "Custom Identification (Block 1) from " + deviceId);
    
    // Parse without lock
    auto identity = SysExParser::parseCustomIdentification(data);
    if (!identity) {
        Logger::warning("SysExHandler", "Failed to parse Custom Identification");
        return;
    }
    
    // Store in cache with short lock
    {
        std::lock_guard<std::mutex> lock(mutex_);
        customIdentityCache_[deviceId] = *identity;
    }
    
    // Log details
    std::stringstream ss;
    ss << "Custom device identified: "
       << "Name='" << identity->deviceName << "'"
       << ", ID=0x" << std::hex << identity->deviceId
       << ", Version=" << std::dec
       << static_cast<int>(identity->firmwareVersion[0]) << "."
       << static_cast<int>(identity->firmwareVersion[1]) << "."
       << static_cast<int>(identity->firmwareVersion[2])
       << ", Features=0x" << std::hex << identity->featureFlags;
    Logger::info("SysExHandler", ss.str());
    
    // Callback without lock
    if (onCustomDeviceIdentified_) {
        onCustomDeviceIdentified_(deviceId, *identity);
    }
}

void SysExHandler::handleNoteMap(const std::vector<uint8_t>& data,
                                 const std::string& deviceId) {
    Logger::info("SysExHandler", "Note Map (Block 2) from " + deviceId);
    
    // Parse without lock
    auto noteMap = SysExParser::parseNoteMap(data);
    if (!noteMap) {
        Logger::warning("SysExHandler", "Failed to parse Note Map");
        return;
    }
    
    // Store in cache with short lock
    {
        std::lock_guard<std::mutex> lock(mutex_);
        noteMapCache_[deviceId] = *noteMap;
    }
    
    // Log details
    std::stringstream ss;
    ss << "Note map received: "
       << "Range=" << static_cast<int>(noteMap->minNote) 
       << "-" << static_cast<int>(noteMap->maxNote)
       << ", Polyphony=" << static_cast<int>(noteMap->polyphony)
       << ", Entries=" << noteMap->mappings.size();
    Logger::info("SysExHandler", ss.str());
    
    // Callback without lock
    if (onNoteMapReceived_) {
        onNoteMapReceived_(deviceId, *noteMap);
    }
}

// ============================================================================
// REQUESTS
// ============================================================================

bool SysExHandler::requestIdentity(const std::string& deviceId) {
    // Standard Identity Request: F0 7E 7F 06 01 F7
    std::vector<uint8_t> request = {
        0xF0,  // SysEx Start
        0x7E,  // Universal Non-Real Time
        0x7F,  // All devices
        0x06,  // General Information
        0x01,  // Identity Request
        0xF7   // SysEx End
    };
    
    if (sendSysEx(deviceId, request)) {
        identityRequestsSent_++;
        Logger::info("SysExHandler", 
            "Identity request sent to " + deviceId);
        return true;
    }
    
    return false;
}

bool SysExHandler::requestCustomIdentification(const std::string& deviceId) {
    // Custom SysEx Request Block 1: F0 7D 00 01 00 F7
    std::vector<uint8_t> request = {
        0xF0,  // SysEx Start
        0x7D,  // Educational/Development use
        0x00,  // MidiMind Manufacturer ID
        0x01,  // Block 1: Identification
        0x00,  // Request (0x00 = request, 0x01 = reply)
        0xF7   // SysEx End
    };
    
    if (sendSysEx(deviceId, request)) {
        Logger::info("SysExHandler",
            "Custom identification request sent to " + deviceId);
        return true;
    }
    
    return false;
}

bool SysExHandler::requestNoteMap(const std::string& deviceId) {
    // Custom SysEx Request Block 2: F0 7D 00 02 00 F7
    std::vector<uint8_t> request = {
        0xF0,  // SysEx Start
        0x7D,  // Educational/Development use
        0x00,  // MidiMind Manufacturer ID
        0x02,  // Block 2: Note Map
        0x00,  // Request
        0xF7   // SysEx End
    };
    
    if (sendSysEx(deviceId, request)) {
        Logger::info("SysExHandler",
            "Note map request sent to " + deviceId);
        return true;
    }
    
    return false;
}

// ============================================================================
// CACHE ACCESS
// ============================================================================

std::optional<DeviceIdentity> SysExHandler::getIdentity(
    const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = identityCache_.find(deviceId);
    if (it != identityCache_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

std::optional<CustomDeviceIdentity> SysExHandler::getCustomIdentity(
    const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = customIdentityCache_.find(deviceId);
    if (it != customIdentityCache_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

std::optional<NoteMap> SysExHandler::getNoteMap(
    const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = noteMapCache_.find(deviceId);
    if (it != noteMapCache_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

void SysExHandler::clearDeviceCache(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    identityCache_.erase(deviceId);
    customIdentityCache_.erase(deviceId);
    noteMapCache_.erase(deviceId);
    
    Logger::debug("SysExHandler", "Cache cleared for " + deviceId);
}

void SysExHandler::clearAllCaches() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    identityCache_.clear();
    customIdentityCache_.clear();
    noteMapCache_.clear();
    
    Logger::info("SysExHandler", "All caches cleared");
}

// ============================================================================
// HELPERS
// ============================================================================

bool SysExHandler::sendSysEx(const std::string& deviceId,
                             const std::vector<uint8_t>& data) {
    if (!onSendMessage_) {
        Logger::error("SysExHandler", 
            "Cannot send SysEx: no send callback configured");
        return false;
    }
    
    // Log sent message
    std::stringstream ss;
    ss << "Sending SysEx to " << deviceId << " (" << data.size() << " bytes): ";
    size_t logLimit = std::min(data.size(), static_cast<size_t>(16));
    for (size_t i = 0; i < logLimit; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<int>(data[i]) << " ";
    }
    if (data.size() > 16) ss << "...";
    Logger::debug("SysExHandler", ss.str());
    
    try {
        onSendMessage_(deviceId, data);
        messagesSent_++;
        return true;
    } catch (const std::exception& e) {
        Logger::error("SysExHandler",
            "Failed to send SysEx: " + std::string(e.what()));
        return false;
    }
}

} // namespace midiMind
// ============================================================================
// File: backend/src/midi/sysex/SysExHandler.h
// Version: 4.1.0
// Project: MidiMind - MIDI Orchestration System for Raspberry Pi
// ============================================================================
//
// Description:
//   Main SysEx message handler. Manages System Exclusive messages including
//   standard Identity Requests and Custom SysEx protocol (0x7D) for DIY
//   instruments. Focuses on Blocks 1-2 for v4.1.0 (Identification + NoteMap).
//
// Dependencies:
//   - SysExParser
//   - nlohmann/json
//
// Author: MidiMind Team
// Date: 2025-10-16
//
// Changes v4.1.0:
//   - Simplified to focus on Blocks 1-2 only
//   - Removed Blocks 3-8 (deferred to later version)
//   - Thread-safe with proper lock ordering
//
// ============================================================================

#pragma once

#include <memory>
#include <functional>
#include <map>
#include <mutex>
#include <atomic>
#include <optional>
#include <vector>
#include <string>

namespace midiMind {

// Forward declarations
struct DeviceIdentity;
struct CustomDeviceIdentity;
struct NoteMap;

/**
 * @class SysExHandler
 * @brief Main handler for System Exclusive messages
 * 
 * @details
 * Processes both standard SysEx (Identity Request/Reply) and Custom SysEx
 * messages (0x7D protocol for DIY instruments). Version 4.1.0 focuses on
 * basic identification and note mapping only.
 * 
 * Thread-safety: Yes (internal mutex)
 * 
 * @example Basic usage
 * ```cpp
 * auto handler = std::make_shared<SysExHandler>();
 * 
 * // Set callback for device identification
 * handler->setOnDeviceIdentified([](const std::string& deviceId, 
 *                                    const DeviceIdentity& id) {
 *     Logger::info("Identified: " + id.manufacturer);
 * });
 * 
 * // Request device identity
 * handler->requestIdentity(deviceId);
 * 
 * // Process received SysEx
 * handler->handleSysExMessage(sysexData, deviceId);
 * ```
 */
class SysExHandler {
public:
    // ========================================================================
    // TYPES
    // ========================================================================
    
    /**
     * @brief Callback for standard device identification
     * @param deviceId Device ID
     * @param identity Parsed device identity
     */
    using DeviceIdentifiedCallback = std::function<void(
        const std::string& deviceId, 
        const DeviceIdentity& identity
    )>;
    
    /**
     * @brief Callback for custom device identification (Block 1)
     * @param deviceId Device ID
     * @param identity Custom device identity
     */
    using CustomDeviceIdentifiedCallback = std::function<void(
        const std::string& deviceId,
        const CustomDeviceIdentity& identity
    )>;
    
    /**
     * @brief Callback for note map received (Block 2)
     * @param deviceId Device ID
     * @param noteMap Note mapping information
     */
    using NoteMapReceivedCallback = std::function<void(
        const std::string& deviceId,
        const NoteMap& noteMap
    )>;
    
    /**
     * @brief Callback to send SysEx message to device
     * @param deviceId Target device ID
     * @param data Raw SysEx data
     */
    using SendMessageCallback = std::function<void(
        const std::string& deviceId,
        const std::vector<uint8_t>& data
    )>;
    
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    SysExHandler();
    ~SysExHandler();
    
    // Non-copyable
    SysExHandler(const SysExHandler&) = delete;
    SysExHandler& operator=(const SysExHandler&) = delete;
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    /**
     * @brief Set callback for sending messages
     * @param callback Function to call when sending SysEx
     */
    void setSendMessageCallback(SendMessageCallback callback) {
        onSendMessage_ = std::move(callback);
    }
    
    /**
     * @brief Set callback for standard device identified
     * @param callback Function to call when device identified
     */
    void setOnDeviceIdentified(DeviceIdentifiedCallback callback) {
        onDeviceIdentified_ = std::move(callback);
    }
    
    /**
     * @brief Set callback for custom device identified (Block 1)
     * @param callback Function to call when custom device identified
     */
    void setOnCustomDeviceIdentified(CustomDeviceIdentifiedCallback callback) {
        onCustomDeviceIdentified_ = std::move(callback);
    }
    
    /**
     * @brief Set callback for note map received (Block 2)
     * @param callback Function to call when note map received
     */
    void setOnNoteMapReceived(NoteMapReceivedCallback callback) {
        onNoteMapReceived_ = std::move(callback);
    }
    
    /**
     * @brief Enable/disable automatic identity request on device connection
     * @param enable True to enable auto-identify
     * @param delayMs Delay in ms before requesting (default: 500ms)
     */
    void setAutoIdentify(bool enable, int delayMs = 500) {
        autoIdentify_ = enable;
        autoIdentifyDelayMs_ = delayMs;
    }
    
    // ========================================================================
    // MESSAGE HANDLING
    // ========================================================================
    
    /**
     * @brief Handle incoming SysEx message
     * @param data Raw MIDI data (including F0...F7)
     * @param deviceId Source device ID
     */
    void handleSysExMessage(const std::vector<uint8_t>& data,
                           const std::string& deviceId);
    
    // ========================================================================
    // REQUESTS
    // ========================================================================
    
    /**
     * @brief Request standard device identity
     * @param deviceId Target device ID
     * @return True if request sent successfully
     * 
     * @details Sends Universal Non-Real Time Identity Request:
     *          F0 7E 7F 06 01 F7
     */
    bool requestIdentity(const std::string& deviceId);
    
    /**
     * @brief Request custom device identification (Block 1)
     * @param deviceId Target device ID
     * @return True if request sent successfully
     * 
     * @details Sends Custom SysEx Request:
     *          F0 7D 00 01 00 F7
     */
    bool requestCustomIdentification(const std::string& deviceId);
    
    /**
     * @brief Request note map (Block 2)
     * @param deviceId Target device ID
     * @return True if request sent successfully
     * 
     * @details Sends Custom SysEx Request:
     *          F0 7D 00 02 00 F7
     */
    bool requestNoteMap(const std::string& deviceId);
    
    // ========================================================================
    // CACHE ACCESS
    // ========================================================================
    
    /**
     * @brief Get cached standard identity
     * @param deviceId Device ID
     * @return Optional identity if found in cache
     */
    std::optional<DeviceIdentity> getIdentity(const std::string& deviceId) const;
    
    /**
     * @brief Get cached custom identity
     * @param deviceId Device ID
     * @return Optional custom identity if found in cache
     */
    std::optional<CustomDeviceIdentity> getCustomIdentity(
        const std::string& deviceId) const;
    
    /**
     * @brief Get cached note map
     * @param deviceId Device ID
     * @return Optional note map if found in cache
     */
    std::optional<NoteMap> getNoteMap(const std::string& deviceId) const;
    
    /**
     * @brief Clear all cached data for a device
     * @param deviceId Device ID
     */
    void clearDeviceCache(const std::string& deviceId);
    
    /**
     * @brief Clear all caches
     */
    void clearAllCaches();
    
    // ========================================================================
    // STATISTICS
    // ========================================================================
    
    /**
     * @brief Get number of messages received
     * @return Message count
     */
    uint32_t getMessagesReceived() const {
        return messagesReceived_.load();
    }
    
    /**
     * @brief Get number of messages sent
     * @return Message count
     */
    uint32_t getMessagesSent() const {
        return messagesSent_.load();
    }
    
    /**
     * @brief Get number of identity replies received
     * @return Reply count
     */
    uint32_t getIdentityRepliesReceived() const {
        return identityRepliesReceived_.load();
    }
    
    /**
     * @brief Reset all statistics
     */
    void resetStatistics() {
        messagesReceived_ = 0;
        messagesSent_ = 0;
        identityRepliesReceived_ = 0;
        identityRequestsSent_ = 0;
    }

private:
    // ========================================================================
    // INTERNAL HANDLERS
    // ========================================================================
    
    void handleIdentityReply(const std::vector<uint8_t>& data,
                            const std::string& deviceId);
    
    void handleCustomIdentification(const std::vector<uint8_t>& data,
                                   const std::string& deviceId);
    
    void handleNoteMap(const std::vector<uint8_t>& data,
                      const std::string& deviceId);
    
    // ========================================================================
    // HELPERS
    // ========================================================================
    
    bool sendSysEx(const std::string& deviceId,
                   const std::vector<uint8_t>& data);
    
    // ========================================================================
    // MEMBER VARIABLES
    // ========================================================================
    
    // Callbacks
    SendMessageCallback onSendMessage_;
    DeviceIdentifiedCallback onDeviceIdentified_;
    CustomDeviceIdentifiedCallback onCustomDeviceIdentified_;
    NoteMapReceivedCallback onNoteMapReceived_;
    
    // Configuration
    bool autoIdentify_;
    int autoIdentifyDelayMs_;
    
    // Caches (protected by mutex_)
    mutable std::mutex mutex_;
    std::map<std::string, DeviceIdentity> identityCache_;
    std::map<std::string, CustomDeviceIdentity> customIdentityCache_;
    std::map<std::string, NoteMap> noteMapCache_;
    
    // Statistics
    std::atomic<uint32_t> messagesReceived_{0};
    std::atomic<uint32_t> messagesSent_{0};
    std::atomic<uint32_t> identityRepliesReceived_{0};
    std::atomic<uint32_t> identityRequestsSent_{0};
};

} // namespace midiMind
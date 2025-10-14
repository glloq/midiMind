// ============================================================================
// Fichier: backend/src/midi/sysex/SysExHandler.cpp
// VERSION CORRIGÉE - Lock Ordering Fixed
// Date: 06/10/2025
// ============================================================================

#include "SysExHandler.h"
#include "../core/Logger.h"

namespace midiMind {

// ============================================================================
// CONSTRUCTION / DESTRUCTION
// ============================================================================

SysExHandler::SysExHandler()
    : autoIdentify_(true)
    , autoIdentifyDelayMs_(500)
    , messagesReceived_(0)
    , messagesSent_(0)
    , identityRepliesReceived_(0)
    , identityRequestsSent_(0)
{
    Logger::info("SysExHandler", "SysExHandler created");
}

SysExHandler::~SysExHandler() {
    Logger::info("SysExHandler", "SysExHandler destroyed");
}

// ============================================================================
// RÉCEPTION DE MESSAGES
// ============================================================================

void SysExHandler::handleSysExMessage(const std::vector<uint8_t>& data, 
                                      const std::string& deviceId) {
    SysExMessage msg(data);
    
    if (!msg.isValid()) {
        Logger::warn("SysExHandler", "Invalid SysEx message from " + deviceId);
        return;
    }
    
    Logger::debug("SysExHandler", 
        "Received message from " + deviceId + 
        " (" + std::to_string(msg.getSize()) + " bytes)");
    
    messagesReceived_++;
    
    // Dispatcher selon le type
    if (SysExParser::isIdentityReply(msg)) {
        handleIdentityReply(msg, deviceId);
    } else if (SysExParser::isGeneralMidi(msg)) {
        handleGeneralMidi(msg, deviceId);
    } else if (SysExParser::isDeviceControl(msg)) {
        handleDeviceControl(msg, deviceId);
    } else if (CustomSysExParser::isCustomSysEx(msg)) {
        handleCustomSysEx(msg, deviceId);
    } else {
        // Message SysEx non géré
        if (onUnhandledSysEx_) {
            onUnhandledSysEx_(deviceId, msg);
        }
        Logger::debug("SysExHandler", "Unhandled SysEx from " + deviceId);
    }
}

// ============================================================================
// HANDLERS SPÉCIFIQUES
// ============================================================================

void SysExHandler::handleIdentityReply(const SysExMessage& msg, 
                                       const std::string& deviceId) {
    Logger::info("SysExHandler", "Identity Reply from " + deviceId);
    
    // ✅ CORRECTION: Parse SANS lock
    auto identity = SysExParser::parseIdentityReply(msg);
    if (!identity) {
        Logger::warn("SysExHandler", "Failed to parse Identity Reply");
        return;
    }
    
    // ✅ Stocker en cache AVEC lock court
    {
        std::lock_guard<std::mutex> lock(mutex_);
        identityCache_[deviceId] = *identity;
        identityRepliesReceived_++;
    }
    
    Logger::info("SysExHandler", "Device identified: " + identity->toString());
    
    // ✅ Callback HORS lock
    if (onDeviceIdentified_) {
        onDeviceIdentified_(deviceId, *identity);
    }
}

void SysExHandler::handleGeneralMidi(const SysExMessage& msg, 
                                    const std::string& deviceId) {
    Logger::debug("SysExHandler", "General MIDI message from " + deviceId);
    // TODO: Implémenter si nécessaire
}

void SysExHandler::handleDeviceControl(const SysExMessage& msg, 
                                       const std::string& deviceId) {
    Logger::debug("SysExHandler", "Device Control message from " + deviceId);
    // TODO: Implémenter si nécessaire
}

void SysExHandler::handleCustomSysEx(const SysExMessage& msg, 
                                     const std::string& deviceId) {
    auto blockId = CustomSysExParser::getBlockId(msg);
    
    if (!blockId) {
        Logger::warn("SysExHandler", "Invalid Custom SysEx from " + deviceId);
        return;
    }
    
    Logger::info("SysExHandler", 
        "Custom SysEx Block " + std::to_string(*blockId) + " from " + deviceId);
    
    switch (*blockId) {
        case CustomSysEx::BLOCK_IDENTIFICATION:
            handleCustomIdentification(msg, deviceId);
            break;
            
        case CustomSysEx::BLOCK_NOTE_MAP:
            handleNoteMap(msg, deviceId);
            break;
            
        case CustomSysEx::BLOCK_CC_SUPPORTED:
            handleCCSupported(msg, deviceId);
            break;
            
        case CustomSysEx::BLOCK_AIR_CAPABILITIES:
            handleAirCapabilities(msg, deviceId);
            break;
            
        case CustomSysEx::BLOCK_LIGHT_CAPABILITIES:
            handleLightCapabilities(msg, deviceId);
            break;
            
        case CustomSysEx::BLOCK_SENSORS_FEEDBACK:
            handleSensorsFeedback(msg, deviceId);
            break;
            
        case CustomSysEx::BLOCK_SYNC_CLOCK:
            handleSyncClock(msg, deviceId);
            break;
            
        default:
            Logger::warn("SysExHandler", 
                "Unknown Custom Block " + std::to_string(*blockId));
            
            auto version = CustomSysExParser::getBlockVersion(msg);
            if (onUnknownCustomBlock_ && version) {
                onUnknownCustomBlock_(deviceId, *blockId, *version, msg);
            }
            break;
    }
}

// ============================================================================
// HANDLERS CUSTOM SYSEX (BLOCS 1-8)
// ============================================================================

void SysExHandler::handleCustomIdentification(const SysExMessage& msg, 
                                              const std::string& deviceId) {
    // ✅ Parse SANS lock
    auto identity = CustomSysExParser::parseIdentification(msg);
    if (!identity) {
        Logger::warn("SysExHandler", "Failed to parse Custom Identification");
        return;
    }
    
    // ✅ Stocker AVEC lock court
    {
        std::lock_guard<std::mutex> lock(mutex_);
        customIdentities_[deviceId] = *identity;
    }
    
    Logger::info("SysExHandler", 
        "Custom Device Identified: " + identity->name + 
        " (ID: 0x" + std::to_string(identity->uniqueId) + ")");
    
    // ✅ Callback HORS lock
    if (onCustomDeviceIdentified_) {
        onCustomDeviceIdentified_(deviceId, *identity);
    }
}

void SysExHandler::handleNoteMap(const SysExMessage& msg, 
                                const std::string& deviceId) {
    // ✅ Parse SANS lock
    auto noteMap = CustomSysExParser::parseNoteMap(msg);
    if (!noteMap) {
        Logger::warn("SysExHandler", "Failed to parse Note Map");
        return;
    }
    
    // Compter les notes jouables
    int playableCount = 0;
    for (int i = 0; i < 128; ++i) {
        if (noteMap->isNotePlayable(i)) {
            playableCount++;
        }
    }
    
    // ✅ Stocker AVEC lock court
    {
        std::lock_guard<std::mutex> lock(mutex_);
        noteMaps_[deviceId] = *noteMap;
    }
    
    Logger::info("SysExHandler", 
        "Note Map: " + std::to_string(playableCount) + " playable notes");
    
    // ✅ Callback HORS lock
    if (onNoteMapReceived_) {
        onNoteMapReceived_(deviceId, *noteMap);
    }
}

void SysExHandler::handleCCSupported(const SysExMessage& msg, 
                                     const std::string& deviceId) {
    // ✅ Parse SANS lock
    auto ccCaps = CustomSysExParser::parseCCSupported(msg);
    if (!ccCaps) {
        Logger::warn("SysExHandler", "Failed to parse CC Supported");
        return;
    }
    
    // ✅ Stocker AVEC lock court
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ccCapabilities_[deviceId] = *ccCaps;
    }
    
    Logger::info("SysExHandler", 
        "CC Capabilities: " + std::to_string(ccCaps->supportedCC.size()) + 
        " controllers");
    
    // ✅ Callback HORS lock
    if (onCCCapabilities_) {
        onCCCapabilities_(deviceId, *ccCaps);
    }
}

void SysExHandler::handleAirCapabilities(const SysExMessage& msg, 
                                        const std::string& deviceId) {
    // ✅ Parse SANS lock
    auto airCaps = CustomSysExParser::parseAirCapabilities(msg);
    if (!airCaps) {
        Logger::warn("SysExHandler", "Failed to parse Air Capabilities");
        return;
    }
    
    // ✅ Stocker AVEC lock court
    {
        std::lock_guard<std::mutex> lock(mutex_);
        airCapabilities_[deviceId] = *airCaps;
    }
    
    Logger::info("SysExHandler", "Air Capabilities received");
    
    // ✅ Callback HORS lock
    if (onAirCapabilities_) {
        onAirCapabilities_(deviceId, *airCaps);
    }
}

void SysExHandler::handleLightCapabilities(const SysExMessage& msg, 
                                          const std::string& deviceId) {
    // ✅ Parse SANS lock
    auto lightCaps = CustomSysExParser::parseLightCapabilities(msg);
    if (!lightCaps) {
        Logger::warn("SysExHandler", "Failed to parse Light Capabilities");
        return;
    }
    
    // ✅ Stocker AVEC lock court
    {
        std::lock_guard<std::mutex> lock(mutex_);
        lightCapabilities_[deviceId] = *lightCaps;
    }
    
    Logger::info("SysExHandler", 
        "Light Capabilities: " + std::to_string(lightCaps->ledCount) + " LEDs");
    
    // ✅ Callback HORS lock
    if (onLightCapabilities_) {
        onLightCapabilities_(deviceId, *lightCaps);
    }
}

void SysExHandler::handleSensorsFeedback(const SysExMessage& msg, 
                                        const std::string& deviceId) {
    // ✅ Parse SANS lock
    auto sensors = CustomSysExParser::parseSensorsFeedback(msg);
    if (!sensors) {
        Logger::warn("SysExHandler", "Failed to parse Sensors Feedback");
        return;
    }
    
    // ✅ Stocker AVEC lock court
    {
        std::lock_guard<std::mutex> lock(mutex_);
        sensorsFeedback_[deviceId] = *sensors;
    }
    
    Logger::info("SysExHandler", 
        "Sensors Feedback: " + std::to_string(sensors->sensors.size()) + 
        " sensors");
    
    // ✅ Callback HORS lock
    if (onSensorsFeedback_) {
        onSensorsFeedback_(deviceId, *sensors);
    }
}

void SysExHandler::handleSyncClock(const SysExMessage& msg, 
                                  const std::string& deviceId) {
    // ✅ Parse SANS lock
    auto sync = CustomSysExParser::parseSyncClock(msg);
    if (!sync) {
        Logger::warn("SysExHandler", "Failed to parse Sync Clock");
        return;
    }
    
    // ✅ Stocker AVEC lock court
    {
        std::lock_guard<std::mutex> lock(mutex_);
        syncClock_[deviceId] = *sync;
    }
    
    Logger::info("SysExHandler", "Sync & Clock capabilities received");
    
    // ✅ Callback HORS lock
    if (onSyncClock_) {
        onSyncClock_(deviceId, *sync);
    }
}

// ============================================================================
// ENVOI DE MESSAGES
// ============================================================================

void SysExHandler::requestIdentity(const std::string& deviceId) {
    Logger::info("SysExHandler", "Requesting identity from " + deviceId);
    
    // Créer le message Identity Request
    SysExMessage msg = SysExBuilder::createIdentityRequest(0x7F);
    
    // Envoyer via callback
    if (onSendSysEx_) {
        onSendSysEx_(deviceId, msg);
        identityRequestsSent_++;
    } else {
        Logger::warn("SysExHandler", "No send callback configured");
    }
}

bool SysExHandler::sendSysEx(const std::string& deviceId, 
                            const SysExMessage& message) {
    if (onSendSysEx_) {
        onSendSysEx_(deviceId, message);
        messagesSent_++;
        return true;
    }
    
    Logger::warn("SysExHandler", "No send callback configured");
    return false;
}

// ============================================================================
// GETTERS - CACHE (THREAD-SAFE)
// ============================================================================

std::optional<DeviceIdentity> SysExHandler::getDeviceIdentity(
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
    
    auto it = customIdentities_.find(deviceId);
    if (it != customIdentities_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

std::optional<NoteMap> SysExHandler::getNoteMap(
    const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = noteMaps_.find(deviceId);
    if (it != noteMaps_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

std::optional<CCCapabilities> SysExHandler::getCCCapabilities(
    const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = ccCapabilities_.find(deviceId);
    if (it != ccCapabilities_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

std::optional<AirCapabilities> SysExHandler::getAirCapabilities(
    const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = airCapabilities_.find(deviceId);
    if (it != airCapabilities_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

std::optional<LightCapabilities> SysExHandler::getLightCapabilities(
    const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = lightCapabilities_.find(deviceId);
    if (it != lightCapabilities_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

std::optional<SensorsFeedback> SysExHandler::getSensorsFeedback(
    const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = sensorsFeedback_.find(deviceId);
    if (it != sensorsFeedback_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

std::optional<SyncClock> SysExHandler::getSyncClock(
    const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = syncClock_.find(deviceId);
    if (it != syncClock_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

void SysExHandler::clearCustomIdentity(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    identityCache_.erase(deviceId);
    customIdentities_.erase(deviceId);
    noteMaps_.erase(deviceId);
    ccCapabilities_.erase(deviceId);
    airCapabilities_.erase(deviceId);
    lightCapabilities_.erase(deviceId);
    sensorsFeedback_.erase(deviceId);
    syncClock_.erase(deviceId);
    
    Logger::info("SysExHandler", "Cleared all cached data for " + deviceId);
}

// ============================================================================
// CONFIGURATION DES CALLBACKS
// ============================================================================

void SysExHandler::setOnDeviceIdentified(DeviceIdentifiedCallback callback) {
    onDeviceIdentified_ = callback;
    Logger::debug("SysExHandler", "Device Identified callback set");
}

void SysExHandler::setOnSendSysEx(SendSysExCallback callback) {
    onSendSysEx_ = callback;
    Logger::debug("SysExHandler", "Send SysEx callback set");
}

void SysExHandler::setOnUnhandledSysEx(UnhandledSysExCallback callback) {
    onUnhandledSysEx_ = callback;
    Logger::debug("SysExHandler", "Unhandled SysEx callback set");
}

void SysExHandler::setOnCustomDeviceIdentified(
    CustomDeviceIdentifiedCallback callback) {
    onCustomDeviceIdentified_ = callback;
    Logger::debug("SysExHandler", "Custom Device Identified callback set");
}

void SysExHandler::setOnNoteMapReceived(NoteMapReceivedCallback callback) {
    onNoteMapReceived_ = callback;
    Logger::debug("SysExHandler", "Note Map Received callback set");
}

void SysExHandler::setOnCCCapabilities(CCCapabilitiesCallback callback) {
    onCCCapabilities_ = callback;
    Logger::debug("SysExHandler", "CC Capabilities callback set");
}

void SysExHandler::setOnAirCapabilities(AirCapabilitiesCallback callback) {
    onAirCapabilities_ = callback;
    Logger::debug("SysExHandler", "Air Capabilities callback set");
}

void SysExHandler::setOnLightCapabilities(LightCapabilitiesCallback callback) {
    onLightCapabilities_ = callback;
    Logger::debug("SysExHandler", "Light Capabilities callback set");
}

void SysExHandler::setOnSensorsFeedback(SensorsFeedbackCallback callback) {
    onSensorsFeedback_ = callback;
    Logger::debug("SysExHandler", "Sensors Feedback callback set");
}

void SysExHandler::setOnSyncClock(SyncClockCallback callback) {
    onSyncClock_ = callback;
    Logger::debug("SysExHandler", "Sync Clock callback set");
}

void SysExHandler::setOnUnknownCustomBlock(UnknownCustomBlockCallback callback) {
    onUnknownCustomBlock_ = callback;
    Logger::debug("SysExHandler", "Unknown Custom Block callback set");
}

// ============================================================================
// CONFIGURATION AUTO-IDENTIFY
// ============================================================================

void SysExHandler::setAutoIdentify(bool enabled) {
    autoIdentify_ = enabled;
    Logger::info("SysExHandler", 
        "Auto-identify " + std::string(enabled ? "enabled" : "disabled"));
}

bool SysExHandler::isAutoIdentifyEnabled() const {
    return autoIdentify_;
}

void SysExHandler::setAutoIdentifyDelay(uint32_t delayMs) {
    autoIdentifyDelayMs_ = delayMs;
    Logger::info("SysExHandler", 
        "Auto-identify delay set to " + std::to_string(delayMs) + "ms");
}

uint32_t SysExHandler::getAutoIdentifyDelay() const {
    return autoIdentifyDelayMs_;
}

// ============================================================================
// STATISTIQUES
// ============================================================================

uint64_t SysExHandler::getMessagesReceived() const {
    return messagesReceived_.load();
}

uint64_t SysExHandler::getMessagesSent() const {
    return messagesSent_.load();
}

uint64_t SysExHandler::getIdentityRepliesReceived() const {
    return identityRepliesReceived_.load();
}

uint64_t SysExHandler::getIdentityRequestsSent() const {
    return identityRequestsSent_.load();
}

json SysExHandler::getStats() const {
    json stats;
    stats["messages_received"] = messagesReceived_.load();
    stats["messages_sent"] = messagesSent_.load();
    stats["identity_replies"] = identityRepliesReceived_.load();
    stats["identity_requests"] = identityRequestsSent_.load();
    
    // Lock court uniquement pour lire les tailles des caches
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stats["cached_identities"] = identityCache_.size();
        stats["cached_custom_identities"] = customIdentities_.size();
        stats["cached_notemaps"] = noteMaps_.size();
        stats["cached_cc_capabilities"] = ccCapabilities_.size();
    }
    
    return stats;
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER SysExHandler.cpp - VERSION CORRIGÉE
// ============================================================================

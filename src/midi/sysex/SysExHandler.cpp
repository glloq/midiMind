// ============================================================================
// Fichier: src/midi/sysex/SysExHandler.cpp
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================

#include "SysExHandler.h"
#include <algorithm>

namespace midiMind {

// ============================================================================
// CONSTRUCTION
// ============================================================================

SysExHandler::SysExHandler()
    : autoIdentify_(true)
    , autoIdentifyDelayMs_(500)
    , messagesReceived_(0)
    , messagesSent_(0)
    , identityRepliesReceived_(0)
    , identityRequestsSent_(0) {
    
    Logger::info("SysExHandler", "SysExHandler constructed");
    Logger::info("SysExHandler", "  Auto-identify: enabled");
    Logger::info("SysExHandler", "  Auto-identify delay: 500ms");
}

SysExHandler::~SysExHandler() {
    Logger::info("SysExHandler", "SysExHandler destroyed");
}

// ============================================================================
// RÉCEPTION DE MESSAGES
// ============================================================================

void SysExHandler::handleSysExMessage(const std::vector<uint8_t>& data, 
                                      const std::string& deviceId) {
    // Créer le message
    SysExMessage message(data);
    
    handleSysExMessage(message, deviceId);
}

void SysExHandler::handleSysExMessage(const SysExMessage& message, 
                                      const std::string& deviceId) {
    messagesReceived_++;
    
    // Vérifier la validité
    if (!message.isValid()) {
        Logger::warn("SysExHandler", "Invalid SysEx message from " + deviceId);
        return;
    }
    
    Logger::debug("SysExHandler", "Received SysEx from " + deviceId + 
                 " (" + std::to_string(message.getSize()) + " bytes)");
    
    // Router selon le type
    if (SysExParser::isIdentityReply(message)) {
        handleIdentityReply(message, deviceId);
    }
    else if (SysExParser::isGeneralMidi(message)) {
        handleGeneralMidi(message, deviceId);
    }
    else if (SysExParser::isDeviceControl(message)) {
        handleDeviceControl(message, deviceId);
    }
    else {
        // Message non géré, appeler le callback
        Logger::debug("SysExHandler", "Unhandled SysEx type from " + deviceId);
        
        if (onUnhandledSysEx_) {
            onUnhandledSysEx_(deviceId, message);
        }
    }
}

// ============================================================================
// IDENTIFICATION DE DEVICES
// ============================================================================

bool SysExHandler::requestIdentity(const std::string& deviceId) {
    Logger::info("SysExHandler", "Requesting identity from device: " + deviceId);
    
    // Créer l'Identity Request
    auto request = SysExBuilder::createIdentityRequest(SysEx::DEVICE_ID_ALL);
    
    // Envoyer
    if (sendSysEx(deviceId, request)) {
        identityRequestsSent_++;
        return true;
    }
    
    Logger::error("SysExHandler", "Failed to send Identity Request to " + deviceId);
    return false;
}

bool SysExHandler::requestIdentityAll() {
    Logger::info("SysExHandler", "Broadcasting Identity Request to all devices");
    
    // Créer l'Identity Request broadcast
    auto request = SysExBuilder::createIdentityRequest(SysEx::DEVICE_ID_ALL);
    
    // Pour un broadcast, on utilise un deviceId spécial
    // L'implémentation dépendra du MidiDeviceManager
    if (sendSysEx("__broadcast__", request)) {
        identityRequestsSent_++;
        return true;
    }
    
    return false;
}

std::optional<DeviceIdentity> SysExHandler::getDeviceIdentity(const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = identityCache_.find(deviceId);
    if (it != identityCache_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

std::map<std::string, DeviceIdentity> SysExHandler::listKnownIdentities() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return identityCache_;
}

void SysExHandler::clearDeviceIdentity(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = identityCache_.find(deviceId);
    if (it != identityCache_.end()) {
        Logger::info("SysExHandler", "Cleared identity for device: " + deviceId);
        identityCache_.erase(it);
    }
}

void SysExHandler::clearAllIdentities() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Logger::info("SysExHandler", "Cleared all device identities (" + 
                std::to_string(identityCache_.size()) + " entries)");
    
    identityCache_.clear();
}

// ============================================================================
// AUTO-IDENTIFICATION
// ============================================================================

void SysExHandler::setAutoIdentify(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    autoIdentify_ = enabled;
    
    Logger::info("SysExHandler", "Auto-identify " + 
                std::string(enabled ? "enabled" : "disabled"));
}

bool SysExHandler::isAutoIdentifyEnabled() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return autoIdentify_;
}

void SysExHandler::setAutoIdentifyDelay(uint32_t delayMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    autoIdentifyDelayMs_ = delayMs;
    
    Logger::info("SysExHandler", "Auto-identify delay set to " + 
                std::to_string(delayMs) + "ms");
}

// ============================================================================
// CALLBACKS
// ============================================================================

void SysExHandler::setOnDeviceIdentified(DeviceIdentifiedCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    onDeviceIdentified_ = callback;
}

void SysExHandler::setOnSendSysEx(SendSysExCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    onSendSysEx_ = callback;
}

void SysExHandler::setOnUnhandledSysEx(UnhandledSysExCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    onUnhandledSysEx_ = callback;
}

// ============================================================================
// CONTRÔLE GÉNÉRAL MIDI
// ============================================================================

bool SysExHandler::sendGMSystemOn(const std::string& deviceId) {
    Logger::info("SysExHandler", "Sending GM System On to " + deviceId);
    
    auto message = SysExBuilder::createGMSystemOn();
    return sendSysEx(deviceId, message);
}

bool SysExHandler::sendGMSystemOff(const std::string& deviceId) {
    Logger::info("SysExHandler", "Sending GM System Off to " + deviceId);
    
    auto message = SysExBuilder::createGMSystemOff();
    return sendSysEx(deviceId, message);
}

bool SysExHandler::sendMasterVolume(const std::string& deviceId, uint16_t volume) {
    Logger::info("SysExHandler", "Sending Master Volume (" + 
                std::to_string(volume) + ") to " + deviceId);
    
    auto message = SysExBuilder::createMasterVolume(SysEx::DEVICE_ID_ALL, volume);
    return sendSysEx(deviceId, message);
}

bool SysExHandler::sendMasterFineTuning(const std::string& deviceId, int16_t cents) {
    Logger::info("SysExHandler", "Sending Master Fine Tuning (" + 
                std::to_string(cents) + " cents) to " + deviceId);
    
    auto message = SysExBuilder::createMasterFineTuning(SysEx::DEVICE_ID_ALL, cents);
    return sendSysEx(deviceId, message);
}

// ============================================================================
// STATISTIQUES
// ============================================================================

json SysExHandler::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    json stats;
    stats["messages_received"] = messagesReceived_.load();
    stats["messages_sent"] = messagesSent_.load();
    stats["identity_replies_received"] = identityRepliesReceived_.load();
    stats["identity_requests_sent"] = identityRequestsSent_.load();
    stats["known_devices"] = identityCache_.size();
    stats["auto_identify_enabled"] = autoIdentify_;
    stats["auto_identify_delay_ms"] = autoIdentifyDelayMs_;
    
    return stats;
}

// ============================================================================
// MÉTHODES PRIVÉES
// ============================================================================

void SysExHandler::handleIdentityReply(const SysExMessage& message, 
                                       const std::string& deviceId) {
    Logger::info("SysExHandler", "Received Identity Reply from " + deviceId);
    
    identityRepliesReceived_++;
    
    // Parser l'Identity Reply
    auto identity = SysExParser::parseIdentityReply(message);
    
    if (!identity.has_value()) {
        Logger::error("SysExHandler", "Failed to parse Identity Reply from " + deviceId);
        return;
    }
    
    // Enrichir avec des infos de la base de données
    auto manufacturerInfo = ManufacturerDatabase::lookup(identity->manufacturer.id[0]);
    if (manufacturerInfo.has_value()) {
        identity->manufacturer = manufacturerInfo.value();
    }
    
    Logger::info("SysExHandler", "Device identified: " + identity->toString());
    Logger::info("SysExHandler", "  Manufacturer: " + identity->manufacturer.name);
    Logger::info("SysExHandler", "  Family: " + std::to_string(identity->familyCode));
    Logger::info("SysExHandler", "  Model: " + std::to_string(identity->modelNumber));
    Logger::info("SysExHandler", "  Firmware: " + identity->firmwareVersion);
    
    // Mettre à jour le cache
    {
        std::lock_guard<std::mutex> lock(mutex_);
        identityCache_[deviceId] = identity.value();
    }
    
    // Callback
    if (onDeviceIdentified_) {
        onDeviceIdentified_(deviceId, identity.value());
    }
}

void SysExHandler::handleGeneralMidi(const SysExMessage& message, 
                                     const std::string& deviceId) {
    auto subId = SysExParser::parseGeneralMidi(message);
    
    if (!subId.has_value()) {
        Logger::warn("SysExHandler", "Failed to parse General MIDI message from " + deviceId);
        return;
    }
    
    switch (subId.value()) {
        case SysEx::GeneralMidi::GM_SYSTEM_ON:
            Logger::info("SysExHandler", "Device " + deviceId + " activated GM mode");
            break;
            
        case SysEx::GeneralMidi::GM_SYSTEM_OFF:
            Logger::info("SysExHandler", "Device " + deviceId + " deactivated GM mode");
            break;
            
        case SysEx::GeneralMidi::GM2_SYSTEM_ON:
            Logger::info("SysExHandler", "Device " + deviceId + " activated GM2 mode");
            break;
            
        default:
            Logger::debug("SysExHandler", "Unknown GM message from " + deviceId + 
                         ": " + std::to_string(subId.value()));
            break;
    }
}

void SysExHandler::handleDeviceControl(const SysExMessage& message, 
                                       const std::string& deviceId) {
    uint8_t subId2 = message.getSubId2();
    
    switch (subId2) {
        case SysEx::DeviceControl::MASTER_VOLUME: {
            auto volume = SysExParser::parseMasterVolume(message);
            if (volume.has_value()) {
                Logger::info("SysExHandler", "Device " + deviceId + " volume: " + 
                            std::to_string(volume.value()));
            }
            break;
        }
        
        case SysEx::DeviceControl::MASTER_FINE_TUNING: {
            auto tuning = SysExParser::parseMasterFineTuning(message);
            if (tuning.has_value()) {
                Logger::info("SysExHandler", "Device " + deviceId + " fine tuning: " + 
                            std::to_string(tuning.value()) + " cents");
            }
            break;
        }
        
        case SysEx::DeviceControl::MASTER_BALANCE:
            Logger::debug("SysExHandler", "Device " + deviceId + " balance change");
            break;
            
        case SysEx::DeviceControl::MASTER_COARSE_TUNING:
            Logger::debug("SysExHandler", "Device " + deviceId + " coarse tuning change");
            break;
            
        default:
            Logger::debug("SysExHandler", "Unknown Device Control from " + deviceId + 
                         ": " + std::to_string(subId2));
            break;
    }
}

bool SysExHandler::sendSysEx(const std::string& deviceId, const SysExMessage& message) {
    if (!onSendSysEx_) {
        Logger::error("SysExHandler", "No send callback configured");
        return false;
    }
    
    try {
        onSendSysEx_(deviceId, message);
        messagesSent_++;
        
        Logger::debug("SysExHandler", "Sent SysEx to " + deviceId + 
                     " (" + std::to_string(message.getSize()) + " bytes)");
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("SysExHandler", "Failed to send SysEx to " + deviceId + 
                     ": " + e.what());
        return false;
    }
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER SysExHandler.cpp
// ============================================================================
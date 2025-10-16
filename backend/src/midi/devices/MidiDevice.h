// ============================================================================
// Fichier: backend/src/midi/devices/MidiDevice.h
// Version: 3.0.3 - CORRECTION handleMessage
// ============================================================================

// CORRECTIFS APPLIQUÉS:
// - ✅ Ligne 252: handleMessage(message) → handleSysExMessage(message)
// ============================================================================

#pragma once

#include "../MidiMessage.h"
#include "../sysex/SysExHandler.h"
#include "../../core/Logger.h"
#include <string>
#include <atomic>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>

namespace midiMind {

using json = nlohmann::json;

// ========================================================================
// ÉNUMÉRATIONS
// ========================================================================

enum class DeviceType {
    USB,
    WIFI,
    BLUETOOTH,
    VIRTUAL,
    UNKNOWN
};

enum class DeviceDirection {
    INPUT,
    OUTPUT,
    BIDIRECTIONAL
};

enum class DeviceStatus {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    ERROR
};

// ========================================================================
// CLASSE ABSTRAITE MIDIDEVICE
// ========================================================================

/**
 * @class MidiDevice
 * @brief Classe abstraite pour tous les périphériques MIDI
 */
class MidiDevice {
public:
    // ========================================================================
    // TYPES
    // ========================================================================
    
    using MessageCallback = std::function<void(const MidiMessage&)>;
    
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    MidiDevice(const std::string& id, 
               const std::string& name, 
               DeviceType type,
               DeviceDirection direction = DeviceDirection::BIDIRECTIONAL)
        : id_(id)
        , name_(name)
        , type_(type)
        , direction_(direction)
        , status_(DeviceStatus::DISCONNECTED)
        , messagesReceived_(0)
        , messagesSent_(0)
    {
        Logger::info("MidiDevice", "Device created: " + name_);
    }
    
    virtual ~MidiDevice() {
        Logger::info("MidiDevice", "Device destroyed: " + name_);
    }
    
    // Non-copiable
    MidiDevice(const MidiDevice&) = delete;
    MidiDevice& operator=(const MidiDevice&) = delete;
    
    // ========================================================================
    // MÉTHODES ABSTRAITES (À IMPLÉMENTER)
    // ========================================================================
    
    /**
     * @brief Connecte le périphérique
     * @return true si succès
     */
    virtual bool connect() = 0;
    
    /**
     * @brief Déconnecte le périphérique
     * @return true si succès
     */
    virtual bool disconnect() = 0;
    
    /**
     * @brief Envoie un message MIDI
     * @param message Message à envoyer
     * @return true si succès
     */
    virtual bool sendMessage(const MidiMessage& message) = 0;
    
    // ========================================================================
    // ACCESSEURS
    // ========================================================================
    
    std::string getId() const { return id_; }
    std::string getName() const { return name_; }
    DeviceType getType() const { return type_; }
    DeviceDirection getDirection() const { return direction_; }
    DeviceStatus getStatus() const { return status_.load(); }
    
    bool isConnected() const {
        return status_.load() == DeviceStatus::CONNECTED;
    }
    
    uint64_t getMessagesReceived() const {
        return messagesReceived_.load();
    }
    
    uint64_t getMessagesSent() const {
        return messagesSent_.load();
    }
    
    // ========================================================================
    // CALLBACKS
    // ========================================================================
    
    void setOnMessageReceived(MessageCallback callback) {
        onMessageReceived_ = callback;
        Logger::debug("MidiDevice", name_ + ": Message callback set");
    }
    
    void setMessageCallback(MessageCallback callback) {
        setOnMessageReceived(callback);
    }
    
    void clearCallback() {
        onMessageReceived_ = nullptr;
        Logger::debug("MidiDevice", name_ + ": Callback cleared");
    }
    
    // ========================================================================
    // SUPPORT SYSEX
    // ========================================================================
    
    void setSysExHandler(std::shared_ptr<SysExHandler> handler) {
        sysexHandler_ = handler;
        Logger::debug("MidiDevice", name_ + ": SysExHandler set");
    }
    
    std::shared_ptr<SysExHandler> getSysExHandler() const {
        return sysexHandler_;
    }
    
    // ========================================================================
    // JSON
    // ========================================================================
    
    virtual json toJson() const {
        json j;
        j["id"] = id_;
        j["name"] = name_;
        j["type"] = deviceTypeToString(type_);
        j["direction"] = directionToString(direction_);
        j["status"] = statusToString(status_.load());
        j["messagesReceived"] = messagesReceived_.load();
        j["messagesSent"] = messagesSent_.load();
        return j;
    }
    
    // ========================================================================
    // UTILITAIRES STATIQUES
    // ========================================================================
    
    static std::string deviceTypeToString(DeviceType type) {
        switch (type) {
            case DeviceType::USB: return "usb";
            case DeviceType::WIFI: return "wifi";
            case DeviceType::BLUETOOTH: return "bluetooth";
            case DeviceType::VIRTUAL: return "virtual";
            default: return "unknown";
        }
    }
    
    static std::string directionToString(DeviceDirection dir) {
        switch (dir) {
            case DeviceDirection::INPUT: return "input";
            case DeviceDirection::OUTPUT: return "output";
            case DeviceDirection::BIDIRECTIONAL: return "bidirectional";
            default: return "unknown";
        }
    }
    
    static std::string statusToString(DeviceStatus status) {
        switch (status) {
            case DeviceStatus::DISCONNECTED: return "disconnected";
            case DeviceStatus::CONNECTING: return "connecting";
            case DeviceStatus::CONNECTED: return "connected";
            case DeviceStatus::ERROR: return "error";
            default: return "unknown";
        }
    }

protected:
    // ========================================================================
    // MÉTHODES PROTÉGÉES POUR CLASSES DÉRIVÉES
    // ========================================================================
    
    /**
     * @brief ✅ CORRECTION: Traite un message MIDI reçu
     * @param message Message reçu
     */
    void handleMessage(const MidiMessage& message) {
        messagesReceived_++;
        
        // ✅ CORRECTION: handleSysExMessage au lieu de handleMessage
        if (message.getType() == MidiMessageType::SYSTEM && sysexHandler_) {
            sysexHandler_->handleSysExMessage(message.getData(), getId());
        }
        
        if (onMessageReceived_) {
            try {
                onMessageReceived_(message);
            } catch (const std::exception& e) {
                Logger::error("MidiDevice", 
                    name_ + ": Callback exception: " + e.what());
            }
        }
    }
    
    void setStatus(DeviceStatus status) {
        status_.store(status);
    }
    
    void incrementMessagesReceived() {
        messagesReceived_++;
    }
    
    void incrementMessagesSent() {
        messagesSent_++;
    }
    
    // ========================================================================
    // MEMBRES PROTÉGÉS
    // ========================================================================
    
    std::string id_;
    std::string name_;
    DeviceType type_;
    DeviceDirection direction_;
    std::atomic<DeviceStatus> status_;
    std::atomic<uint64_t> messagesReceived_;
    std::atomic<uint64_t> messagesSent_;
    MessageCallback onMessageReceived_;
    std::shared_ptr<SysExHandler> sysexHandler_;
};

using MidiDevicePtr = std::shared_ptr<MidiDevice>;

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MidiDevice.h
// ============================================================================

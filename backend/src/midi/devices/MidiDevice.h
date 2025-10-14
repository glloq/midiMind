// ============================================================================
// Fichier: src/midi/devices/MidiDevice.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================

#pragma once
#include "../MidiMessage.h"
#include <string>
#include <atomic>
#include <functional>
#include <nlohmann/json.hpp>
#include "../sysex/SysExHandler.h"

namespace midiMind {

using json = nlohmann::json;

// ============================================================================
// ÉNUMÉRATIONS
// ============================================================================

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

// ============================================================================
// CLASSE DE BASE MidiDevice
// ============================================================================

class MidiDevice {
public:
    MidiDevice(const std::string& id, const std::string& name, DeviceType type,
               DeviceDirection direction = DeviceDirection::BIDIRECTIONAL)
        : id_(id)
        , name_(name) 
        , type_(type)
        , direction_(direction)
        , status_(DeviceStatus::DISCONNECTED)
        , messagesReceived_(0)
        , messagesSent_(0) {
    }
    
    virtual ~MidiDevice() = default;
    
    // Méthodes virtuelles pures
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool sendMessage(const MidiMessage& msg) = 0;
    
    // Méthodes virtuelles avec implémentation par défaut
    virtual bool open() { return connect(); }
    virtual void close() { disconnect(); }
    virtual void send(const MidiMessage& msg) { sendMessage(msg); }
    virtual MidiMessage receive() { return MidiMessage(); }
    virtual bool hasMessages() const { return false; }
    virtual std::string getPort() const { return ""; }
    virtual json getInfo() const {
        json info;
        info["id"] = id_;
        info["name"] = name_;
        info["type"] = static_cast<int>(type_);
        info["status"] = static_cast<int>(status_.load());
        info["messages_received"] = messagesReceived_.load();
        info["messages_sent"] = messagesSent_.load();
        return info;
    }
    
    // Getters
    std::string getId() const { return id_; }
    std::string getName() const { return name_; }
    DeviceType getType() const { return type_; }
    DeviceDirection getDirection() const { return direction_; }
    DeviceStatus getStatus() const { return status_; }
    
    bool isOpen() const { return status_ == DeviceStatus::CONNECTED; }
    bool isConnected() const { return status_ == DeviceStatus::CONNECTED; }
    
	/**
     * @brief Définit le SysExHandler pour ce device
     * 
     * @param handler Handler SysEx partagé
     */
    void setSysExHandler(std::shared_ptr<SysExHandler> handler) {
        sysexHandler_ = handler;
    }
	
	
	
    // Callbacks
    void setOnMessageReceived(std::function<void(const MidiMessage&)> callback) {
        onMessageReceived_ = callback;
    }
    
protected:

/// Référence au SysExHandler pour traiter les messages SysEx
    std::shared_ptr<SysExHandler> sysexHandler_;
	/**
     * @brief Traite un message MIDI reçu
     */
    void handleMessage(const MidiMessage& message) override {
        // Si c'est un message SysEx
        if (message.getType() == MidiMessageType::SYSTEM_EXCLUSIVE) {
            if (sysexHandler_) {
                // Transférer au SysExHandler
                sysexHandler_->handleSysExMessage(
                    message.getData(), 
                    getId()
                );
            }
        }
        
        // Notifier les listeners (comportement existant)
        notifyListeners(message);
    }
	
    void setStatus(DeviceStatus status) { status_ = status; }
    void incrementMessagesReceived() { messagesReceived_++; }
    void incrementMessagesSent() { messagesSent_++; }
    
    std::string id_;
    std::string name_;
    DeviceType type_;
    DeviceDirection direction_;
    std::atomic<DeviceStatus> status_;
    
    std::atomic<uint64_t> messagesReceived_;
    std::atomic<uint64_t> messagesSent_;
    
    std::function<void(const MidiMessage&)> onMessageReceived_;
};

} // namespace midiMind
// ============================================================================
// Fichier: src/midi/devices/VirtualMidiDevice.cpp
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================

#include "VirtualMidiDevice.h"

namespace midiMind {

// ============================================================================
// CONSTRUCTION / DESTRUCTION
// ============================================================================

VirtualMidiDevice::VirtualMidiDevice(const std::string& name)
    : name_(name)
    , virtualPort_("virtual:" + name)
    , isInput_(true)
    , isOutput_(true)
    , isOpen_(false)
    , alsaHandle_(nullptr) {
    
    // Créer les files lock-free (1024 messages max)
    inputQueue_ = std::make_unique<LockFreeQueue<MidiMessage>>(1024);
    outputQueue_ = std::make_unique<LockFreeQueue<MidiMessage>>(1024);
    
    Logger::info("VirtualMidiDevice", "Virtual device created: " + name_);
}

VirtualMidiDevice::~VirtualMidiDevice() {
    close();
    Logger::info("VirtualMidiDevice", "Virtual device destroyed: " + name_);
}

// ============================================================================
// OUVERTURE / FERMETURE
// ============================================================================

bool VirtualMidiDevice::open() {
    if (isOpen_) {
        Logger::warn("VirtualMidiDevice", "Already open: " + name_);
        return true;
    }
    
    Logger::info("VirtualMidiDevice", "Opening virtual port: " + name_);
    
    // TODO: Créer le port ALSA virtuel si disponible
    // Pour l'instant, mode simulation avec files internes
    
    isOpen_ = true;
    
    Logger::info("VirtualMidiDevice", "✓ Virtual port opened: " + name_);
    
    return true;
}

void VirtualMidiDevice::close() {
    if (!isOpen_) {
        return;
    }
    
    Logger::info("VirtualMidiDevice", "Closing virtual port: " + name_);
    
    // TODO: Fermer le port ALSA
    
    // Vider les files
    clearMessages();
    
    isOpen_ = false;
    
    Logger::info("VirtualMidiDevice", "✓ Virtual port closed: " + name_);
}

bool VirtualMidiDevice::isOpen() const {
    return isOpen_;
}

// ============================================================================
// ENVOI / RÉCEPTION
// ============================================================================

void VirtualMidiDevice::send(const MidiMessage& message) {
    if (!isOpen_) {
        Logger::warn("VirtualMidiDevice", "Cannot send: port not open");
        return;
    }
    
    if (!isOutput_) {
        Logger::warn("VirtualMidiDevice", "Cannot send: port is input-only");
        return;
    }
    
    // Ajouter à la file de sortie
    if (!outputQueue_->push(message)) {
        Logger::warn("VirtualMidiDevice", "Output queue full, message dropped");
    }
}

MidiMessage VirtualMidiDevice::receive() {
    if (!isOpen_) {
        return MidiMessage();
    }
    
    if (!isInput_) {
        return MidiMessage();
    }
    
    // Récupérer de la file d'entrée
    auto msg = inputQueue_->pop();
    return msg.value_or(MidiMessage());
}

bool VirtualMidiDevice::hasMessages() const {
    if (!isOpen_ || !isInput_) {
        return false;
    }
    
    return !inputQueue_->isEmpty();
}

// ============================================================================
// INFORMATIONS
// ============================================================================

json VirtualMidiDevice::getInfo() const {
    json info;
    
    info["name"] = name_;
    info["type"] = "VIRTUAL";
    info["port"] = virtualPort_;
    info["is_open"] = isOpen_.load();
    info["is_input"] = isInput_;
    info["is_output"] = isOutput_;
    info["input_queue_size"] = inputQueue_->size();
    info["output_queue_size"] = outputQueue_->size();
    
    return info;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void VirtualMidiDevice::setPortDirection(bool input, bool output) {
    isInput_ = input;
    isOutput_ = output;
    
    Logger::info("VirtualMidiDevice", "Port direction set: " +
                std::string(input ? "IN" : "") +
                std::string(input && output ? "+" : "") +
                std::string(output ? "OUT" : ""));
}

size_t VirtualMidiDevice::getMessageCount() const {
    return inputQueue_->size();
}

void VirtualMidiDevice::clearMessages() {
    // Vider les files
    while (!inputQueue_->isEmpty()) {
        inputQueue_->pop();
    }
    
    while (!outputQueue_->isEmpty()) {
        outputQueue_->pop();
    }
    
    Logger::debug("VirtualMidiDevice", "Message queues cleared");
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER VirtualMidiDevice.cpp
// ============================================================================
// ============================================================================
// Fichier: src/midi/devices/VirtualMidiDevice.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Device MIDI virtuel pour routage interne.
//   Permet de créer des ports MIDI virtuels sans matériel physique.
//
// Responsabilités:
//   - Créer des ports MIDI virtuels
//   - Communication inter-processus
//   - Routage interne
//
// Thread-safety: OUI
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include "MidiDevice.h"
#include "../../core/optimization/LockFreeQueue.h"

namespace midiMind {

/**
 * @class VirtualMidiDevice
 * @brief Device MIDI virtuel
 * 
 * @details
 * Crée un port MIDI virtuel pour le routage interne ou la communication
 * avec d'autres applications MIDI sur le système.
 * 
 * Utilise ALSA Sequencer pour créer des ports virtuels visibles par
 * d'autres applications MIDI (DAW, synthés logiciels, etc.).
 * 
 * Thread-safety: Toutes les méthodes publiques sont thread-safe.
 * 
 * @example Utilisation
 * ```cpp
 * auto virtualDevice = std::make_shared<VirtualMidiDevice>("MidiMind Virtual");
 * 
 * // Ouvrir le port virtuel
 * virtualDevice->open();
 * 
 * // Envoyer un message
 * MidiMessage msg = MidiMessage::noteOn(1, 60, 100);
 * virtualDevice->send(msg);
 * ```
 */
class VirtualMidiDevice : public MidiDevice {
public:
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     * 
     * @param name Nom du port virtuel
     */
    explicit VirtualMidiDevice(const std::string& name);
    
    /**
     * @brief Destructeur
     */
    ~VirtualMidiDevice() override;
    
    // ========================================================================
    // IMPLÉMENTATION MidiDevice
    // ========================================================================
    
    bool open() override;
    void close() override;
    bool isOpen() const override;
    
    void send(const MidiMessage& message) override;
    MidiMessage receive() override;
    bool hasMessages() const override;
    
    DeviceType getType() const override { return DeviceType::VIRTUAL; }
    std::string getName() const override { return name_; }
    std::string getPort() const override { return virtualPort_; }
    
    json getInfo() const override;
    
    // ========================================================================
    // SPÉCIFIQUE VIRTUAL
    // ========================================================================
    
    /**
     * @brief Définit si le port est Input, Output ou Both
     * 
     * @param input true pour Input
     * @param output true pour Output
     */
    void setPortDirection(bool input, bool output);
    
    /**
     * @brief Récupère le nombre de messages en attente
     */
    size_t getMessageCount() const;
    
    /**
     * @brief Vide la file de messages
     */
    void clearMessages();

private:
    // ========================================================================
    // MEMBRES PRIVÉS
    // ========================================================================
    
    /// Nom du port virtuel
    std::string name_;
    
    /// Port virtuel (nom système)
    std::string virtualPort_;
    
    /// File de messages entrants (lock-free)
    std::unique_ptr<LockFreeQueue<MidiMessage>> inputQueue_;
    
    /// File de messages sortants (lock-free)
    std::unique_ptr<LockFreeQueue<MidiMessage>> outputQueue_;
    
    /// Direction du port
    bool isInput_;
    bool isOutput_;
    
    /// État d'ouverture
    std::atomic<bool> isOpen_;
    
    /// Handle ALSA (optionnel)
    void* alsaHandle_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER VirtualMidiDevice.h
// ============================================================================
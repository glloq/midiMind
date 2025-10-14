// ============================================================================
// Fichier: src/midi/processing/basic/TransposeProcessor.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Processeur de transposition MIDI.
//   Transpose les notes MIDI par un nombre de demi-tons.
//
// Thread-safety: Oui
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include "../MidiProcessor.h"
#include <algorithm>

namespace midiMind {

/**
 * @class TransposeProcessor
 * @brief Processeur de transposition
 * 
 * @details
 * Transpose les notes MIDI par un nombre de demi-tons.
 * Limite les notes à la plage MIDI valide (0-127).
 * 
 * Paramètres:
 * - semitones: Nombre de demi-tons (-24 à +24)
 * 
 * Thread-safety: Oui
 * 
 * @example Utilisation
 * ```cpp
 * auto transpose = std::make_shared<TransposeProcessor>();
 * transpose->setSemitones(7); // Transpose de 5 demi-tons (quinte)
 * 
 * auto output = transpose->process(noteOn);
 * ```
 */
class TransposeProcessor : public MidiProcessor {
public:
    // ========================================================================
    // CONSTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     * 
     * @param semitones Transposition initiale (défaut: 0)
     */
    explicit TransposeProcessor(int semitones = 0)
        : MidiProcessor("Transpose", ProcessorType::TRANSPOSE)
        , semitones_(semitones) {
        
        parameters_["semitones"] = semitones;
    }
    
    // ========================================================================
    // TRAITEMENT
    // ========================================================================
    
    /**
     * @brief Traite un message MIDI
     */
    std::vector<MidiMessage> process(const MidiMessage& input) override {
        // Bypass
        if (!isEnabled() || isBypassed() || semitones_ == 0) {
            return {input};
        }
        
        // Ne traiter que les Note On/Off
        if (!input.isNoteOn() && !input.isNoteOff()) {
            return {input};
        }
        
        // Transposer la note
        int newNote = input.getNote() + semitones_;
        
        // Limiter à la plage MIDI valide
        if (newNote < 0 || newNote > 127) {
            // Note hors plage, filtrer le message
            return {};
        }
        
        // Créer le message transposé
        MidiMessage output = input;
        output.setNote(static_cast<uint8_t>(newNote));
        
        return {output};
    }
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    /**
     * @brief Définit la transposition
     * 
     * @param semitones Nombre de demi-tons (-24 à +24)
     */
    void setSemitones(int semitones) {
        // Limiter à ±24 demi-tons (2 octaves)
        semitones_ = std::clamp(semitones, -24, 24);
        parameters_["semitones"] = semitones_;
    }
    
    /**
     * @brief Récupère la transposition actuelle
     */
    int getSemitones() const {
        return semitones_;
    }
    
    /**
     * @brief Définit un paramètre
     */
    bool setParameter(const std::string& name, const json& value) override {
        if (name == "semitones") {
            setSemitones(value.get<int>());
            return true;
        }
        
        return MidiProcessor::setParameter(name, value);
    }

private:
    /// Transposition en demi-tons
    int semitones_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER TransposeProcessor.h
// ============================================================================
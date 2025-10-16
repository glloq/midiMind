// ============================================================================
// Fichier: backend/src/midi/processing/basic/TransposeProcessor.h
// Version: 3.0.1 - COMPLET ET À JOUR
// ============================================================================

// NOTE: Utilise setNote() de MidiMessage.h (corrigé)
// ============================================================================

#pragma once

#include "../MidiProcessor.h"

namespace midiMind {

/**
 * @class TransposeProcessor
 * @brief Transpose les notes MIDI
 */
class TransposeProcessor : public MidiProcessor {
public:
    // ========================================================================
    // CONSTRUCTION
    // ========================================================================
    
    TransposeProcessor(const std::string& id = "transpose", 
                      const std::string& name = "Transpose")
        : MidiProcessor(id, name)
    {
        type_ = "transpose";
        
        // Paramètres
        registerParameter("semitones", 0);  // -12 à +12
        registerParameter("octaves", 0);    // -2 à +2
    }
    
    // ========================================================================
    // IMPLÉMENTATION MIDIPROCESSOR
    // ========================================================================
    
    std::vector<MidiMessage> process(const MidiMessage& message) override {
        if (!enabled_) {
            incrementBypassed();
            return {message};
        }
        
        // Ne traiter que les notes
        if (!message.isNote()) {
            return {message};
        }
        
        int semitones = parameters_["semitones"].get<int>();
        int octaves = parameters_["octaves"].get<int>();
        int totalTranspose = semitones + (octaves * 12);
        
        if (totalTranspose == 0) {
            return {message};
        }
        
        // Transposer
        int originalNote = message.getNote();
        int newNote = originalNote + totalTranspose;
        
        // Clamp à 0-127
        if (newNote < 0) newNote = 0;
        if (newNote > 127) newNote = 127;
        
        // ✅ Utilise setNote() de MidiMessage.h corrigé
        MidiMessage output = message;
        output.setNote(static_cast<uint8_t>(newNote));
        
        incrementProcessed();
        
        return {output};
    }
    
    void reset() override {
        // Rien à réinitialiser
    }
    
    std::unique_ptr<MidiProcessor> clone() const override {
        auto cloned = std::make_unique<TransposeProcessor>(id_, name_);
        cloned->loadParameters(parameters_);
        cloned->setEnabled(enabled_);
        return cloned;
    }
    
    // ========================================================================
    // MÉTHODES SPÉCIFIQUES
    // ========================================================================
    
    /**
     * @brief Définit la transposition en demi-tons
     * @param semitones Nombre de demi-tons (-12 à +12)
     */
    void setSemitones(int semitones) {
        if (semitones < -12) semitones = -12;
        if (semitones > 12) semitones = 12;
        setParameter("semitones", semitones);
    }
    
    /**
     * @brief Récupère la transposition en demi-tons
     */
    int getSemitones() const {
        return parameters_["semitones"].get<int>();
    }
    
    /**
     * @brief Définit la transposition en octaves
     * @param octaves Nombre d'octaves (-2 à +2)
     */
    void setOctaves(int octaves) {
        if (octaves < -2) octaves = -2;
        if (octaves > 2) octaves = 2;
        setParameter("octaves", octaves);
    }
    
    /**
     * @brief Récupère la transposition en octaves
     */
    int getOctaves() const {
        return parameters_["octaves"].get<int>();
    }
    
    /**
     * @brief Récupère la transposition totale en demi-tons
     */
    int getTotalTranspose() const {
        return getSemitones() + (getOctaves() * 12);
    }
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER TransposeProcessor.h
// ============================================================================

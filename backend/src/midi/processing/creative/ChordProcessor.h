// ============================================================================
// Fichier: src/midi/processing/creative/ChordProcessor.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Processeur de génération d'accords.
//   Transforme les notes simples en accords.
//
// Thread-safety: Oui
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include "../MidiProcessor.h"
#include <map>

namespace midiMind {

/**
 * @enum ChordType
 * @brief Type d'accord
 */
enum class ChordType {
    MAJOR,              ///< Majeur (0, 4, 7)
    MINOR,              ///< Mineur (0, 3, 7)
    DIMINISHED,         ///< Diminué (0, 3, 6)
    AUGMENTED,          ///< Augmenté (0, 4, 8)
    MAJOR7,             ///< Majeur 7 (0, 4, 7, 11)
    MINOR7,             ///< Mineur 7 (0, 3, 7, 10)
    DOMINANT7,          ///< Dominante 7 (0, 4, 7, 10)
    MAJOR6,             ///< Majeur 6 (0, 4, 7, 9)
    MINOR6,             ///< Mineur 6 (0, 3, 7, 9)
    SUS2,               ///< Sus2 (0, 2, 7)
    SUS4,               ///< Sus4 (0, 5, 7)
    POWER,              ///< Power chord (0, 7)
    OCTAVE,             ///< Octave (0, 12)
    FIFTH               ///< Quinte (0, 7, 12)
};

/**
 * @class ChordProcessor
 * @brief Processeur de génération d'accords
 * 
 * @details
 * Transforme chaque note jouée en accord complet.
 * Supporte de nombreux types d'accords.
 * 
 * Paramètres:
 * - chord_type: Type d'accord
 * - velocity_scale: Échelle de vélocité pour les notes ajoutées (0.0-1.0)
 * - inversion: Inversion de l'accord (0=fondamental, 1=1ère, 2=2ème)
 * 
 * Thread-safety: Oui
 * 
 * @example Utilisation
 * ```cpp
 * auto chord = std::make_shared<ChordProcessor>();
 * chord->setChordType(ChordType::MAJOR7);
 * chord->setVelocityScale(0.8f);
 * 
 * // Note Do → Accord Do maj7
 * ```
 */
class ChordProcessor : public MidiProcessor {
public:
    // ========================================================================
    // CONSTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     * 
     * @param type Type d'accord initial
     */
    explicit ChordProcessor(ChordType type = ChordType::MAJOR)
        : MidiProcessor("Chord", ProcessorType::CHORD)
        , chordType_(type)
        , velocityScale_(0.8f)
        , inversion_(0) {
        
        parameters_["chord_type"] = static_cast<int>(type);
        parameters_["velocity_scale"] = velocityScale_;
        parameters_["inversion"] = inversion_;
        
        initializeChordIntervals();
    }
    
    // ========================================================================
    // TRAITEMENT
    // ========================================================================
    
    /**
     * @brief Traite un message MIDI
     */
    std::vector<MidiMessage> process(const MidiMessage& input) override {
        // Bypass
        if (!isEnabled() || isBypassed()) {
            return {input};
        }
        
        // Ne traiter que les Note On/Off
        if (!input.isNoteOn() && !input.isNoteOff()) {
            return {input};
        }
        
        std::vector<MidiMessage> output;
        
        // Note originale
        output.push_back(input);
        
        // Si c'est un Note On, générer l'accord
        if (input.isNoteOn()) {
            auto intervals = getChordIntervals(chordType_);
            
            // Appliquer l'inversion
            if (inversion_ > 0 && inversion_ < intervals.size()) {
                std::rotate(intervals.begin(), 
                           intervals.begin() + inversion_, 
                           intervals.end());
                
                // Ajuster les octaves pour l'inversion
                for (size_t i = 0; i < inversion_; ++i) {
                    intervals[i] += 12;
                }
            }
            
            // Générer les notes de l'accord
            uint8_t rootNote = input.getNote();
            uint8_t baseVelocity = input.getVelocity();
            uint8_t chordVelocity = static_cast<uint8_t>(baseVelocity * velocityScale_);
            
            for (size_t i = 1; i < intervals.size(); ++i) {
                int note = rootNote + intervals[i];
                
                // Vérifier que la note est dans la plage MIDI
                if (note >= 0 && note <= 127) {
                    MidiMessage chordNote = MidiMessage::noteOn(
                        input.getChannel(),
                        static_cast<uint8_t>(note),
                        chordVelocity
                    );
                    
                    output.push_back(chordNote);
                    
                    // Mémoriser pour le Note Off
                    activeChordNotes_[rootNote].push_back(static_cast<uint8_t>(note));
                }
            }
        }
        // Si c'est un Note Off, éteindre toutes les notes de l'accord
        else if (input.isNoteOff()) {
            uint8_t rootNote = input.getNote();
            
            auto it = activeChordNotes_.find(rootNote);
            if (it != activeChordNotes_.end()) {
                for (uint8_t note : it->second) {
                    MidiMessage noteOff = MidiMessage::noteOff(
                        input.getChannel(),
                        note,
                        0
                    );
                    
                    output.push_back(noteOff);
                }
                
                activeChordNotes_.erase(it);
            }
        }
        
        return output;
    }
    
    /**
     * @brief Réinitialise l'état
     */
    void reset() override {
        activeChordNotes_.clear();
    }
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    /**
     * @brief Définit le type d'accord
     */
    void setChordType(ChordType type) {
        chordType_ = type;
        parameters_["chord_type"] = static_cast<int>(type);
    }
    
    /**
     * @brief Récupère le type d'accord
     */
    ChordType getChordType() const {
        return chordType_;
    }
    
    /**
     * @brief Définit l'échelle de vélocité
     * 
     * @param scale Facteur (0.0-1.0)
     */
    void setVelocityScale(float scale) {
        velocityScale_ = std::clamp(scale, 0.0f, 1.0f);
        parameters_["velocity_scale"] = velocityScale_;
    }
    
    /**
     * @brief Récupère l'échelle de vélocité
     */
    float getVelocityScale() const {
        return velocityScale_;
    }
    
    /**
     * @brief Définit l'inversion
     * 
     * @param inversion Numéro d'inversion (0=fondamental)
     */
    void setInversion(uint8_t inversion) {
        inversion_ = std::min(inversion, uint8_t(3));
        parameters_["inversion"] = inversion_;
    }
    
    /**
     * @brief Récupère l'inversion
     */
    uint8_t getInversion() const {
        return inversion_;
    }
    
    /**
     * @brief Définit un paramètre
     */
    bool setParameter(const std::string& name, const json& value) override {
        if (name == "chord_type") {
            setChordType(static_cast<ChordType>(value.get<int>()));
            return true;
        } else if (name == "velocity_scale") {
            setVelocityScale(value.get<float>());
            return true;
        } else if (name == "inversion") {
            setInversion(value.get<uint8_t>());
            return true;
        }
        
        return MidiProcessor::setParameter(name, value);
    }

private:
    /**
     * @brief Initialise les intervalles d'accords
     */
    void initializeChordIntervals() {
        chordIntervals_[ChordType::MAJOR] = {0, 4, 7};
        chordIntervals_[ChordType::MINOR] = {0, 3, 7};
        chordIntervals_[ChordType::DIMINISHED] = {0, 3, 6};
        chordIntervals_[ChordType::AUGMENTED] = {0, 4, 8};
        chordIntervals_[ChordType::MAJOR7] = {0, 4, 7, 11};
        chordIntervals_[ChordType::MINOR7] = {0, 3, 7, 10};
        chordIntervals_[ChordType::DOMINANT7] = {0, 4, 7, 10};
        chordIntervals_[ChordType::MAJOR6] = {0, 4, 7, 9};
        chordIntervals_[ChordType::MINOR6] = {0, 3, 7, 9};
        chordIntervals_[ChordType::SUS2] = {0, 2, 7};
        chordIntervals_[ChordType::SUS4] = {0, 5, 7};
        chordIntervals_[ChordType::POWER] = {0, 7};
        chordIntervals_[ChordType::OCTAVE] = {0, 12};
        chordIntervals_[ChordType::FIFTH] = {0, 7, 12};
    }
    
    /**
     * @brief Récupère les intervalles d'un type d'accord
     */
    std::vector<int> getChordIntervals(ChordType type) const {
        auto it = chordIntervals_.find(type);
        if (it != chordIntervals_.end()) {
            return it->second;
        }
        return {0}; // Fallback: juste la note fondamentale
    }
    
    /// Type d'accord
    ChordType chordType_;
    
    /// Échelle de vélocité pour les notes ajoutées
    float velocityScale_;
    
    /// Inversion de l'accord
    uint8_t inversion_;
    
    /// Table des intervalles par type d'accord
    std::map<ChordType, std::vector<int>> chordIntervals_;
    
    /// Notes d'accords actives (pour le Note Off)
    /// Map: note fondamentale → notes de l'accord
    std::map<uint8_t, std::vector<uint8_t>> activeChordNotes_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER ChordProcessor.h
// ============================================================================
// ============================================================================
// Fichier: src/midi/processing/creative/HarmonizerProcessor.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Processeur d'harmonisation MIDI.
//   Ajoute des voix harmoniques selon une gamme/tonalité.
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
 * @enum Scale
 * @brief Type de gamme
 */
enum class Scale {
    MAJOR,              ///< Majeure
    MINOR_NATURAL,      ///< Mineure naturelle
    MINOR_HARMONIC,     ///< Mineure harmonique
    MINOR_MELODIC,      ///< Mineure mélodique
    DORIAN,             ///< Dorien
    PHRYGIAN,           ///< Phrygien
    LYDIAN,             ///< Lydien
    MIXOLYDIAN,         ///< Mixolydien
    LOCRIAN,            ///< Locrien
    PENTATONIC_MAJOR,   ///< Pentatonique majeure
    PENTATONIC_MINOR,   ///< Pentatonique mineure
    BLUES,              ///< Blues
    CHROMATIC           ///< Chromatique
};

/**
 * @class HarmonizerProcessor
 * @brief Processeur d'harmonisation
 * 
 * @details
 * Ajoute des voix harmoniques intelligentes basées sur une gamme.
 * Respecte la tonalité et crée des harmonies musicalement correctes.
 * 
 * Paramètres:
 * - scale: Gamme utilisée
 * - key: Tonique (0-11, Do=0)
 * - intervals: Intervalles des voix (ex: [3, 7] = tierce + quinte)
 * - velocity_scale: Échelle de vélocité pour les harmonies
 * 
 * Thread-safety: Oui
 * 
 * @example Utilisation
 * ```cpp
 * auto harmonizer = std::make_shared<HarmonizerProcessor>();
 * harmonizer->setScale(Scale::MAJOR);
 * harmonizer->setKey(0); // Do majeur
 * harmonizer->addInterval(4); // Tierce majeure
 * harmonizer->addInterval(7); // Quinte juste
 * ```
 */
class HarmonizerProcessor : public MidiProcessor {
public:
    // ========================================================================
    // CONSTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     */
    HarmonizerProcessor()
        : MidiProcessor("Harmonizer", ProcessorType::HARMONIZER)
        , scale_(Scale::MAJOR)
        , key_(0)
        , velocityScale_(0.8f) {
        
        parameters_["scale"] = static_cast<int>(scale_);
        parameters_["key"] = key_;
        parameters_["velocity_scale"] = velocityScale_;
        
        initializeScales();
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
        
        // Si c'est un Note On, générer les harmonies
        if (input.isNoteOn() && !intervals_.empty()) {
            uint8_t rootNote = input.getNote();
            uint8_t baseVelocity = input.getVelocity();
            uint8_t harmonyVelocity = static_cast<uint8_t>(baseVelocity * velocityScale_);
            
            for (int interval : intervals_) {
                int harmonyNote = quantizeToScale(rootNote + interval);
                
                // Vérifier que la note est dans la plage MIDI
                if (harmonyNote >= 0 && harmonyNote <= 127) {
                    MidiMessage harmony = MidiMessage::noteOn(
                        input.getChannel(),
                        static_cast<uint8_t>(harmonyNote),
                        harmonyVelocity
                    );
                    
                    output.push_back(harmony);
                    
                    // Mémoriser pour le Note Off
                    activeHarmonies_[rootNote].push_back(static_cast<uint8_t>(harmonyNote));
                }
            }
        }
        // Si c'est un Note Off, éteindre les harmonies
        else if (input.isNoteOff()) {
            uint8_t rootNote = input.getNote();
            
            auto it = activeHarmonies_.find(rootNote);
            if (it != activeHarmonies_.end()) {
                for (uint8_t note : it->second) {
                    MidiMessage noteOff = MidiMessage::noteOff(
                        input.getChannel(),
                        note,
                        0
                    );
                    
                    output.push_back(noteOff);
                }
                
                activeHarmonies_.erase(it);
            }
        }
        
        return output;
    }
    
    /**
     * @brief Réinitialise l'état
     */
    void reset() override {
        activeHarmonies_.clear();
    }
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    /**
     * @brief Définit la gamme
     */
    void setScale(Scale scale) {
        scale_ = scale;
        parameters_["scale"] = static_cast<int>(scale);
    }
    
    /**
     * @brief Récupère la gamme
     */
    Scale getScale() const {
        return scale_;
    }
    
    /**
     * @brief Définit la tonique
     * 
     * @param key Note (0-11, Do=0, Do#=1, etc.)
     */
    void setKey(uint8_t key) {
        key_ = key % 12;
        parameters_["key"] = key_;
    }
    
    /**
     * @brief Récupère la tonique
     */
    uint8_t getKey() const {
        return key_;
    }
    
    /**
     * @brief Ajoute un intervalle harmonique
     * 
     * @param interval Intervalle en demi-tons
     */
    void addInterval(int interval) {
        intervals_.push_back(interval);
        updateIntervalsParameter();
    }
    
    /**
     * @brief Retire un intervalle
     */
    void removeInterval(int interval) {
        auto it = std::find(intervals_.begin(), intervals_.end(), interval);
        if (it != intervals_.end()) {
            intervals_.erase(it);
            updateIntervalsParameter();
        }
    }
    
    /**
     * @brief Efface tous les intervalles
     */
    void clearIntervals() {
        intervals_.clear();
        updateIntervalsParameter();
    }
    
    /**
     * @brief Définit l'échelle de vélocité
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
     * @brief Définit un paramètre
     */
    bool setParameter(const std::string& name, const json& value) override {
        if (name == "scale") {
            setScale(static_cast<Scale>(value.get<int>()));
            return true;
        } else if (name == "key") {
            setKey(value.get<uint8_t>());
            return true;
        } else if (name == "velocity_scale") {
            setVelocityScale(value.get<float>());
            return true;
        } else if (name == "intervals") {
            intervals_.clear();
            if (value.is_array()) {
                for (const auto& interval : value) {
                    addInterval(interval.get<int>());
                }
            }
            return true;
        }
        
        return MidiProcessor::setParameter(name, value);
    }

private:
    /**
     * @brief Initialise les gammes
     */
    void initializeScales() {
        // Intervalles en demi-tons depuis la tonique
        scaleIntervals_[Scale::MAJOR] = {0, 2, 4, 5, 7, 9, 11};
        scaleIntervals_[Scale::MINOR_NATURAL] = {0, 2, 3, 5, 7, 8, 10};
        scaleIntervals_[Scale::MINOR_HARMONIC] = {0, 2, 3, 5, 7, 8, 11};
        scaleIntervals_[Scale::MINOR_MELODIC] = {0, 2, 3, 5, 7, 9, 11};
        scaleIntervals_[Scale::DORIAN] = {0, 2, 3, 5, 7, 9, 10};
        scaleIntervals_[Scale::PHRYGIAN] = {0, 1, 3, 5, 7, 8, 10};
        scaleIntervals_[Scale::LYDIAN] = {0, 2, 4, 6, 7, 9, 11};
        scaleIntervals_[Scale::MIXOLYDIAN] = {0, 2, 4, 5, 7, 9, 10};
        scaleIntervals_[Scale::LOCRIAN] = {0, 1, 3, 5, 6, 8, 10};
        scaleIntervals_[Scale::PENTATONIC_MAJOR] = {0, 2, 4, 7, 9};
        scaleIntervals_[Scale::PENTATONIC_MINOR] = {0, 3, 5, 7, 10};
        scaleIntervals_[Scale::BLUES] = {0, 3, 5, 6, 7, 10};
        scaleIntervals_[Scale::CHROMATIC] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    }
    
    /**
     * @brief Quantise une note à la gamme
     */
    int quantizeToScale(int note) const {
        // Obtenir la note relative à la tonique
        int relativeNote = (note - key_ + 120) % 12;
        
        // Trouver la note la plus proche dans la gamme
        auto& intervals = scaleIntervals_.at(scale_);
        
        int closestInterval = intervals[0];
        int minDistance = std::abs(relativeNote - closestInterval);
        
        for (int interval : intervals) {
            int distance = std::abs(relativeNote - interval);
            if (distance < minDistance) {
                minDistance = distance;
                closestInterval = interval;
            }
        }
        
        // Recalculer la note absolue
        int octave = (note - key_) / 12;
        return key_ + octave * 12 + closestInterval;
    }
    
    /**
     * @brief Met à jour le paramètre intervals
     */
    void updateIntervalsParameter() {
        json intervalsArray = json::array();
        for (int interval : intervals_) {
            intervalsArray.push_back(interval);
        }
        parameters_["intervals"] = intervalsArray;
    }
    
    /// Gamme utilisée
    Scale scale_;
    
    /// Tonique (0-11)
    uint8_t key_;
    
    /// Intervalles harmoniques
    std::vector<int> intervals_;
    
    /// Échelle de vélocité
    float velocityScale_;
    
    /// Table des intervalles par gamme
    std::map<Scale, std::vector<int>> scaleIntervals_;
    
    /// Harmonies actives
    std::map<uint8_t, std::vector<uint8_t>> activeHarmonies_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER HarmonizerProcessor.h
// ============================================================================
// ============================================================================
// Fichier: src/midi/processing/basic/VelocityProcessor.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Processeur de modification de vélocité MIDI.
//   Ajuste la vélocité des notes (volume/intensité).
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
#include <cmath>

namespace midiMind {

/**
 * @enum VelocityMode
 * @brief Mode de modification de la vélocité
 */
enum class VelocityMode {
    MULTIPLY,       ///< Multiplication (0.0 - 2.0)
    ADD,            ///< Addition (-127 à +127)
    SET,            ///< Valeur fixe (0-127)
    COMPRESS,       ///< Compression dynamique
    EXPAND          ///< Expansion dynamique
};

/**
 * @class VelocityProcessor
 * @brief Processeur de vélocité
 * 
 * @details
 * Modifie la vélocité des notes MIDI selon différents modes:
 * - Multiply: Multiplie par un facteur
 * - Add: Ajoute/soustrait une valeur
 * - Set: Fixe à une valeur constante
 * - Compress: Réduit la dynamique
 * - Expand: Augmente la dynamique
 * 
 * Paramètres:
 * - mode: Mode de modification
 * - value: Valeur selon le mode
 * - threshold: Seuil pour compression/expansion
 * 
 * Thread-safety: Oui
 * 
 * @example Utilisation
 * ```cpp
 * auto velocity = std::make_shared<VelocityProcessor>();
 * velocity->setMode(VelocityMode::MULTIPLY);
 * velocity->setValue(1.5f); // +50% de volume
 * ```
 */
class VelocityProcessor : public MidiProcessor {
public:
    // ========================================================================
    // CONSTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     * 
     * @param mode Mode de modification
     * @param value Valeur initiale
     */
    VelocityProcessor(VelocityMode mode = VelocityMode::MULTIPLY, float value = 1.0f)
        : MidiProcessor("Velocity", ProcessorType::VELOCITY)
        , mode_(mode)
        , value_(value)
        , threshold_(64) {
        
        parameters_["mode"] = static_cast<int>(mode);
        parameters_["value"] = value;
        parameters_["threshold"] = threshold_;
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
        
        // Ne traiter que les Note On
        if (!input.isNoteOn()) {
            return {input};
        }
        
        uint8_t originalVelocity = input.getVelocity();
        uint8_t newVelocity = originalVelocity;
        
        switch (mode_) {
            case VelocityMode::MULTIPLY:
                newVelocity = static_cast<uint8_t>(
                    std::clamp(originalVelocity * value_, 0.0f, 127.0f)
                );
                break;
                
            case VelocityMode::ADD:
                newVelocity = static_cast<uint8_t>(
                    std::clamp(originalVelocity + static_cast<int>(value_), 0, 127)
                );
                break;
                
            case VelocityMode::SET:
                newVelocity = static_cast<uint8_t>(
                    std::clamp(static_cast<int>(value_), 0, 127)
                );
                break;
                
            case VelocityMode::COMPRESS:
                // Compression: Réduit les valeurs au-dessus du seuil
                if (originalVelocity > threshold_) {
                    float excess = originalVelocity - threshold_;
                    newVelocity = threshold_ + static_cast<uint8_t>(excess * value_);
                } else {
                    newVelocity = originalVelocity;
                }
                break;
                
            case VelocityMode::EXPAND:
                // Expansion: Augmente les valeurs au-dessus du seuil
                if (originalVelocity > threshold_) {
                    float excess = originalVelocity - threshold_;
                    newVelocity = std::min(127, 
                        static_cast<int>(threshold_ + excess * value_)
                    );
                } else {
                    newVelocity = originalVelocity;
                }
                break;
        }
        
        // Filtrer les vélocités nulles (Note Off)
        if (newVelocity == 0) {
            return {};
        }
        
        // Créer le message modifié
        MidiMessage output = input;
        output.setVelocity(newVelocity);
        
        return {output};
    }
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    /**
     * @brief Définit le mode
     */
    void setMode(VelocityMode mode) {
        mode_ = mode;
        parameters_["mode"] = static_cast<int>(mode);
    }
    
    /**
     * @brief Récupère le mode
     */
    VelocityMode getMode() const {
        return mode_;
    }
    
    /**
     * @brief Définit la valeur
     */
    void setValue(float value) {
        value_ = value;
        parameters_["value"] = value;
    }
    
    /**
     * @brief Récupère la valeur
     */
    float getValue() const {
        return value_;
    }
    
    /**
     * @brief Définit le seuil (pour compression/expansion)
     */
    void setThreshold(uint8_t threshold) {
        threshold_ = std::clamp(threshold, uint8_t(0), uint8_t(127));
        parameters_["threshold"] = threshold_;
    }
    
    /**
     * @brief Récupère le seuil
     */
    uint8_t getThreshold() const {
        return threshold_;
    }
    
    /**
     * @brief Définit un paramètre
     */
    bool setParameter(const std::string& name, const json& value) override {
        if (name == "mode") {
            setMode(static_cast<VelocityMode>(value.get<int>()));
            return true;
        } else if (name == "value") {
            setValue(value.get<float>());
            return true;
        } else if (name == "threshold") {
            setThreshold(value.get<uint8_t>());
            return true;
        }
        
        return MidiProcessor::setParameter(name, value);
    }

private:
    /// Mode de modification
    VelocityMode mode_;
    
    /// Valeur (dépend du mode)
    float value_;
    
    /// Seuil pour compression/expansion
    uint8_t threshold_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER VelocityProcessor.h
// ============================================================================
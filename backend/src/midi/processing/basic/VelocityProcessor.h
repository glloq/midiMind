// ============================================================================
// Fichier: backend/src/midi/processing/basic/VelocityProcessor.h
// Version: 3.0.1 - COMPLET ET À JOUR
// ============================================================================

// NOTE: Utilise setVelocity() de MidiMessage.h (corrigé)
// ============================================================================

#pragma once

#include "../MidiProcessor.h"
#include <cmath>

namespace midiMind {

/**
 * @class VelocityProcessor
 * @brief Modifie la vélocité des notes MIDI
 */
class VelocityProcessor : public MidiProcessor {
public:
    // ========================================================================
    // CONSTRUCTION
    // ========================================================================
    
    VelocityProcessor(const std::string& id = "velocity", 
                     const std::string& name = "Velocity")
        : MidiProcessor(id, name)
    {
        type_ = "velocity";
        
        // Paramètres
        registerParameter("mode", "scale");     // "scale", "add", "set", "curve"
        registerParameter("amount", 100);       // 0-200 pour scale, -127 à +127 pour add, 0-127 pour set
        registerParameter("curve", 1.0);        // 0.1-10.0 pour curve
        registerParameter("min", 1);            // Vélocité minimum
        registerParameter("max", 127);          // Vélocité maximum
        registerParameter("randomize", 0);      // 0-100% de randomisation
    }
    
    // ========================================================================
    // IMPLÉMENTATION MIDIPROCESSOR
    // ========================================================================
    
    std::vector<MidiMessage> process(const MidiMessage& message) override {
        if (!enabled_) {
            incrementBypassed();
            return {message};
        }
        
        // Ne traiter que les Note On
        if (!message.isNoteOn()) {
            return {message};
        }
        
        int originalVelocity = message.getVelocity();
        int newVelocity = originalVelocity;
        
        std::string mode = parameters_["mode"].get<std::string>();
        
        if (mode == "scale") {
            // Mise à l'échelle (100 = pas de changement)
            int amount = parameters_["amount"].get<int>();
            newVelocity = (originalVelocity * amount) / 100;
            
        } else if (mode == "add") {
            // Ajout/soustraction
            int amount = parameters_["amount"].get<int>();
            newVelocity = originalVelocity + amount;
            
        } else if (mode == "set") {
            // Valeur fixe
            newVelocity = parameters_["amount"].get<int>();
            
        } else if (mode == "curve") {
            // Courbe exponentielle
            double curve = parameters_["curve"].get<double>();
            double normalized = originalVelocity / 127.0;
            normalized = std::pow(normalized, curve);
            newVelocity = static_cast<int>(normalized * 127.0);
        }
        
        // Randomisation
        int randomize = parameters_["randomize"].get<int>();
        if (randomize > 0) {
            int range = (randomize * 127) / 100;
            int random = (std::rand() % (range * 2 + 1)) - range;
            newVelocity += random;
        }
        
        // Clamp entre min et max
        int minVel = parameters_["min"].get<int>();
        int maxVel = parameters_["max"].get<int>();
        
        if (newVelocity < minVel) newVelocity = minVel;
        if (newVelocity > maxVel) newVelocity = maxVel;
        
        // Clamp absolu 1-127 (vélocité 0 = Note Off)
        if (newVelocity < 1) newVelocity = 1;
        if (newVelocity > 127) newVelocity = 127;
        
        // ✅ Utilise setVelocity() de MidiMessage.h corrigé
        MidiMessage output = message;
        output.setVelocity(static_cast<uint8_t>(newVelocity));
        
        incrementProcessed();
        
        return {output};
    }
    
    void reset() override {
        // Rien à réinitialiser
    }
    
    std::unique_ptr<MidiProcessor> clone() const override {
        auto cloned = std::make_unique<VelocityProcessor>(id_, name_);
        cloned->loadParameters(parameters_);
        cloned->setEnabled(enabled_);
        return cloned;
    }
    
    // ========================================================================
    // MÉTHODES SPÉCIFIQUES
    // ========================================================================
    
    /**
     * @brief Définit le mode de modification
     * @param mode "scale", "add", "set", ou "curve"
     */
    void setMode(const std::string& mode) {
        if (mode == "scale" || mode == "add" || mode == "set" || mode == "curve") {
            setParameter("mode", mode);
        }
    }
    
    /**
     * @brief Récupère le mode actuel
     */
    std::string getMode() const {
        return parameters_["mode"].get<std::string>();
    }
    
    /**
     * @brief Définit la quantité de modification
     */
    void setAmount(int amount) {
        setParameter("amount", amount);
    }
    
    /**
     * @brief Récupère la quantité
     */
    int getAmount() const {
        return parameters_["amount"].get<int>();
    }
    
    /**
     * @brief Définit la courbe (mode curve)
     * @param curve 0.1-10.0 (< 1.0 = plus doux, > 1.0 = plus fort)
     */
    void setCurve(double curve) {
        if (curve < 0.1) curve = 0.1;
        if (curve > 10.0) curve = 10.0;
        setParameter("curve", curve);
    }
    
    /**
     * @brief Récupère la courbe
     */
    double getCurve() const {
        return parameters_["curve"].get<double>();
    }
    
    /**
     * @brief Définit la vélocité minimum
     */
    void setMinVelocity(int min) {
        if (min < 1) min = 1;
        if (min > 127) min = 127;
        setParameter("min", min);
    }
    
    /**
     * @brief Définit la vélocité maximum
     */
    void setMaxVelocity(int max) {
        if (max < 1) max = 1;
        if (max > 127) max = 127;
        setParameter("max", max);
    }
    
    /**
     * @brief Définit le pourcentage de randomisation
     * @param percent 0-100
     */
    void setRandomize(int percent) {
        if (percent < 0) percent = 0;
        if (percent > 100) percent = 100;
        setParameter("randomize", percent);
    }
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER VelocityProcessor.h
// ============================================================================

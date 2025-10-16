// ============================================================================
// Fichier: backend/src/midi/processing/MidiProcessor.h
// Version: 3.0.2 - CORRECTION ITERATOR JSON
// ============================================================================

// CORRECTIFS APPLIQUÉS:
// - ✅ Ligne 255: Correction it->second → it.value() pour json iterator
// ============================================================================

#pragma once

#include "../MidiMessage.h"
#include "../../core/Logger.h"
#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

namespace midiMind {

using json = nlohmann::json;

/**
 * @class MidiProcessor
 * @brief Classe abstraite pour processeurs MIDI
 */
class MidiProcessor {
public:
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    MidiProcessor(const std::string& id, const std::string& name)
        : id_(id)
        , name_(name)
        , enabled_(true)
        , bypassedMessageCount_(0)
        , processedMessageCount_(0)
    {
        Logger::debug("MidiProcessor", "Processor created: " + name_);
    }
    
    virtual ~MidiProcessor() {
        Logger::debug("MidiProcessor", "Processor destroyed: " + name_);
    }
    
    // Non-copiable
    MidiProcessor(const MidiProcessor&) = delete;
    MidiProcessor& operator=(const MidiProcessor&) = delete;
    
    // ========================================================================
    // MÉTHODES ABSTRAITES
    // ========================================================================
    
    /**
     * @brief Traite un message MIDI
     * @param message Message à traiter
     * @return Messages de sortie (peut être vide, un, ou plusieurs)
     */
    virtual std::vector<MidiMessage> process(const MidiMessage& message) = 0;
    
    /**
     * @brief Réinitialise l'état du processeur
     */
    virtual void reset() = 0;
    
    /**
     * @brief Clone le processeur
     */
    virtual std::unique_ptr<MidiProcessor> clone() const = 0;
    
    // ========================================================================
    // ACCESSEURS
    // ========================================================================
    
    std::string getId() const { return id_; }
    std::string getName() const { return name_; }
    std::string getType() const { return type_; }
    
    bool isEnabled() const { return enabled_; }
    void setEnabled(bool enabled) { enabled_ = enabled; }
    
    uint64_t getProcessedMessageCount() const { return processedMessageCount_; }
    uint64_t getBypassedMessageCount() const { return bypassedMessageCount_; }
    
    // ========================================================================
    // PARAMÈTRES
    // ========================================================================
    
    /**
     * @brief Définit un paramètre
     * @param name Nom du paramètre
     * @param value Valeur JSON
     * @return true si succès
     */
    virtual bool setParameter(const std::string& name, const json& value) {
        if (parameters_.contains(name)) {
            parameters_[name] = value;
            onParameterChanged(name, value);
            return true;
        }
        return false;
    }
    
    /**
     * @brief ✅ CORRECTION: Récupère un paramètre
     * @param name Nom du paramètre
     * @return Valeur JSON ou null
     */
    virtual json getParameter(const std::string& name) const {
        auto it = parameters_.find(name);
        if (it != parameters_.end()) {
            // ✅ CORRECTION: Utilise it.value() au lieu de it->second
            return it.value();
        }
        return json();
    }
    
    /**
     * @brief Récupère tous les paramètres
     */
    json getParameters() const {
        return parameters_;
    }
    
    /**
     * @brief Charge les paramètres depuis JSON
     */
    virtual bool loadParameters(const json& params) {
        if (!params.is_object()) {
            return false;
        }
        
        for (auto it = params.begin(); it != params.end(); ++it) {
            setParameter(it.key(), it.value());
        }
        
        return true;
    }
    
    // ========================================================================
    // SÉRIALISATION
    // ========================================================================
    
    /**
     * @brief Exporte en JSON
     */
    virtual json toJson() const {
        json j;
        j["id"] = id_;
        j["name"] = name_;
        j["type"] = type_;
        j["enabled"] = enabled_;
        j["parameters"] = parameters_;
        j["stats"] = {
            {"processed", processedMessageCount_},
            {"bypassed", bypassedMessageCount_}
        };
        return j;
    }
    
    /**
     * @brief Charge depuis JSON
     */
    virtual bool fromJson(const json& j) {
        if (!j.is_object()) {
            return false;
        }
        
        if (j.contains("enabled")) {
            enabled_ = j["enabled"];
        }
        
        if (j.contains("parameters")) {
            loadParameters(j["parameters"]);
        }
        
        return true;
    }
    
    // ========================================================================
    // STATISTIQUES
    // ========================================================================
    
    /**
     * @brief Réinitialise les statistiques
     */
    void resetStats() {
        processedMessageCount_ = 0;
        bypassedMessageCount_ = 0;
    }

protected:
    // ========================================================================
    // MÉTHODES PROTÉGÉES
    // ========================================================================
    
    /**
     * @brief Appelé quand un paramètre change
     */
    virtual void onParameterChanged(const std::string& name, const json& value) {
        // À implémenter dans les classes dérivées si nécessaire
    }
    
    /**
     * @brief Enregistre un paramètre avec valeur par défaut
     */
    void registerParameter(const std::string& name, const json& defaultValue) {
        parameters_[name] = defaultValue;
    }
    
    /**
     * @brief Incrémente le compteur de messages traités
     */
    void incrementProcessed() {
        processedMessageCount_++;
    }
    
    /**
     * @brief Incrémente le compteur de messages bypassés
     */
    void incrementBypassed() {
        bypassedMessageCount_++;
    }
    
    // ========================================================================
    // MEMBRES PROTÉGÉS
    // ========================================================================
    
    std::string id_;
    std::string name_;
    std::string type_;
    bool enabled_;
    json parameters_;
    
    uint64_t processedMessageCount_;
    uint64_t bypassedMessageCount_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MidiProcessor.h
// ============================================================================

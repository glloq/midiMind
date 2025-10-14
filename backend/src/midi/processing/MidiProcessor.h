// ============================================================================
// Fichier: src/midi/processing/MidiProcessor.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Interface de base pour tous les processeurs MIDI.
//   Définit le contrat que doivent respecter tous les effets MIDI.
//
// Responsabilités:
//   - Définir l'API commune de traitement
//   - Gérer l'état actif/inactif
//   - Gérer les paramètres
//   - Supporter le bypass
//
// Thread-safety: Les implémentations doivent être thread-safe
//
// Patterns: Strategy Pattern, Template Method Pattern
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>
#include "../MidiMessage.h"

using json = nlohmann::json;

namespace midiMind {

/**
 * @enum ProcessorType
 * @brief Type de processeur MIDI
 */
enum class ProcessorType {
    TRANSPOSE,          ///< Transposition
    VELOCITY,           ///< Modification de vélocité
    CHANNEL_FILTER,     ///< Filtrage par canal
    NOTE_FILTER,        ///< Filtrage par note
    ARPEGGIATOR,        ///< Arpégiateur
    DELAY,              ///< Délai MIDI
    CHORD,              ///< Générateur d'accords
    HARMONIZER,         ///< Harmonisation
    CUSTOM              ///< Processeur personnalisé
};

/**
 * @class MidiProcessor
 * @brief Interface de base pour les processeurs MIDI
 * 
 * @details
 * Tous les processeurs MIDI doivent hériter de cette classe.
 * Chaque processeur transforme des messages MIDI en entrée
 * et produit des messages MIDI en sortie.
 * 
 * Un processeur peut :
 * - Modifier les messages (transpose, velocity)
 * - Filtrer les messages (channel filter, note filter)
 * - Générer de nouveaux messages (arpeggiator, chord)
 * - Retarder les messages (delay)
 * 
 * Thread-safety: Les implémentations doivent être thread-safe.
 * 
 * @example Implémentation d'un processeur
 * ```cpp
 * class MyProcessor : public MidiProcessor {
 * public:
 *     MyProcessor() : MidiProcessor("MyProcessor", ProcessorType::CUSTOM) {}
 *     
 *     std::vector<MidiMessage> process(const MidiMessage& input) override {
 *         if (!isEnabled()) {
 *             return {input}; // Bypass
 *         }
 *         
 *         // Traitement...
 *         return {modifiedMessage};
 *     }
 * };
 * ```
 */
class MidiProcessor {
public:
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     * 
     * @param name Nom du processeur
     * @param type Type du processeur
     */
    MidiProcessor(const std::string& name, ProcessorType type)
        : name_(name)
        , type_(type)
        , enabled_(true)
        , bypassed_(false) {}
    
    /**
     * @brief Destructeur virtuel
     */
    virtual ~MidiProcessor() = default;
    
    // ========================================================================
    // TRAITEMENT (MÉTHODE PRINCIPALE)
    // ========================================================================
    
    /**
     * @brief Traite un message MIDI
     * 
     * Méthode principale de traitement. Peut retourner :
     * - Un vecteur vide (message filtré)
     * - Un message (transformation 1:1)
     * - Plusieurs messages (génération)
     * 
     * @param input Message MIDI en entrée
     * @return std::vector<MidiMessage> Messages en sortie
     * 
     * @note Doit être thread-safe
     */
    virtual std::vector<MidiMessage> process(const MidiMessage& input) = 0;
    
    /**
     * @brief Traite plusieurs messages en batch
     * 
     * Optimisation pour traiter plusieurs messages d'un coup.
     * Par défaut, appelle process() pour chaque message.
     * 
     * @param inputs Messages en entrée
     * @return std::vector<MidiMessage> Messages en sortie
     */
    virtual std::vector<MidiMessage> processBatch(const std::vector<MidiMessage>& inputs) {
        std::vector<MidiMessage> outputs;
        
        for (const auto& input : inputs) {
            auto result = process(input);
            outputs.insert(outputs.end(), result.begin(), result.end());
        }
        
        return outputs;
    }
    
    // ========================================================================
    // ÉTAT
    // ========================================================================
    
    /**
     * @brief Active/désactive le processeur
     * 
     * @param enabled true pour activer
     */
    virtual void setEnabled(bool enabled) {
        enabled_ = enabled;
    }
    
    /**
     * @brief Vérifie si le processeur est actif
     */
    bool isEnabled() const {
        return enabled_;
    }
    
    /**
     * @brief Active/désactive le bypass
     * 
     * En mode bypass, les messages passent sans traitement.
     * 
     * @param bypassed true pour bypasser
     */
    virtual void setBypassed(bool bypassed) {
        bypassed_ = bypassed;
    }
    
    /**
     * @brief Vérifie si le processeur est bypassé
     */
    bool isBypassed() const {
        return bypassed_;
    }
    
    /**
     * @brief Réinitialise l'état du processeur
     * 
     * Réinitialise tous les états internes (buffers, compteurs, etc.)
     * Appelé lors d'un changement de configuration ou de tempo.
     */
    virtual void reset() {}
    
    // ========================================================================
    // INFORMATIONS
    // ========================================================================
    
    /**
     * @brief Récupère le nom du processeur
     */
    std::string getName() const {
        return name_;
    }
    
    /**
     * @brief Récupère le type du processeur
     */
    ProcessorType getType() const {
        return type_;
    }
    
    /**
     * @brief Récupère le type sous forme de string
     */
    std::string getTypeString() const {
        switch (type_) {
            case ProcessorType::TRANSPOSE: return "Transpose";
            case ProcessorType::VELOCITY: return "Velocity";
            case ProcessorType::CHANNEL_FILTER: return "ChannelFilter";
            case ProcessorType::NOTE_FILTER: return "NoteFilter";
            case ProcessorType::ARPEGGIATOR: return "Arpeggiator";
            case ProcessorType::DELAY: return "Delay";
            case ProcessorType::CHORD: return "Chord";
            case ProcessorType::HARMONIZER: return "Harmonizer";
            case ProcessorType::CUSTOM: return "Custom";
            default: return "Unknown";
        }
    }
    
    // ========================================================================
    // PARAMÈTRES
    // ========================================================================
    
    /**
     * @brief Définit un paramètre
     * 
     * @param name Nom du paramètre
     * @param value Valeur (int, float, string, etc.)
     * @return true Si le paramètre a été défini
     */
    virtual bool setParameter(const std::string& name, const json& value) {
        parameters_[name] = value;
        return true;
    }
    
    /**
     * @brief Récupère un paramètre
     * 
     * @param name Nom du paramètre
     * @return json Valeur du paramètre
     */
    virtual json getParameter(const std::string& name) const {
        auto it = parameters_.find(name);
        if (it != parameters_.end()) {
            return it->second;
        }
        return json();
    }
    
    /**
     * @brief Liste tous les paramètres
     * 
     * @return json Objet JSON avec tous les paramètres
     */
    virtual json getParameters() const {
        return parameters_;
    }
    
    /**
     * @brief Définit plusieurs paramètres d'un coup
     * 
     * @param params Objet JSON avec les paramètres
     */
    virtual void setParameters(const json& params) {
        if (params.is_object()) {
            for (auto& [key, value] : params.items()) {
                setParameter(key, value);
            }
        }
    }
    
    // ========================================================================
    // SÉRIALISATION
    // ========================================================================
    
    /**
     * @brief Convertit en JSON
     * 
     * @return json Configuration complète du processeur
     */
    virtual json toJson() const {
        json j;
        j["name"] = name_;
        j["type"] = getTypeString();
        j["enabled"] = enabled_;
        j["bypassed"] = bypassed_;
        j["parameters"] = parameters_;
        return j;
    }
    
    /**
     * @brief Configure depuis JSON
     * 
     * @param j Configuration JSON
     */
    virtual void fromJson(const json& j) {
        if (j.contains("enabled")) {
            enabled_ = j["enabled"].get<bool>();
        }
        
        if (j.contains("bypassed")) {
            bypassed_ = j["bypassed"].get<bool>();
        }
        
        if (j.contains("parameters")) {
            setParameters(j["parameters"]);
        }
    }

protected:
    /// Nom du processeur
    std::string name_;
    
    /// Type du processeur
    ProcessorType type_;
    
    /// État actif/inactif
    bool enabled_;
    
    /// État bypass
    bool bypassed_;
    
    /// Paramètres du processeur
    json parameters_;
};

/**
 * @brief Alias pour un shared_ptr de MidiProcessor
 */
using MidiProcessorPtr = std::shared_ptr<MidiProcessor>;

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER MidiProcessor.h
// ============================================================================
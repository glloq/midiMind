// ============================================================================
// Fichier: src/midi/processing/ProcessorChain.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Chaîne de processeurs MIDI.
//   Permet de chaîner plusieurs processors pour créer des effets complexes.
//
// Responsabilités:
//   - Gérer l'ordre des processors
//   - Router les messages à travers la chaîne
//   - Gérer l'activation/bypass individuel
//   - Sauvegarder/charger les configurations
//
// Thread-safety: OUI
//
// Patterns: Chain of Responsibility Pattern
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include <vector>
#include <memory>
#include <mutex>
#include <string>
#include <nlohmann/json.hpp>

#include "MidiProcessor.h"
#include "../MidiMessage.h"
#include "../../core/Logger.h"

using json = nlohmann::json;

namespace midiMind {

/**
 * @class ProcessorChain
 * @brief Chaîne de processeurs MIDI
 * 
 * @details
 * Permet de chaîner plusieurs processors MIDI.
 * Les messages sont traités séquentiellement par chaque processor.
 * 
 * Exemple de chaîne:
 * Input → Transpose → Velocity → Chord → Delay → Output
 * 
 * Thread-safety: Toutes les méthodes publiques sont thread-safe.
 * 
 * @example Utilisation
 * ```cpp
 * ProcessorChain chain("MyChain");
 * 
 * chain.addProcessor(std::make_shared<TransposeProcessor>(5));
 * chain.addProcessor(std::make_shared<ChordProcessor>());
 * chain.addProcessor(std::make_shared<DelayProcessor>());
 * 
 * // Traiter un message
 * auto outputs = chain.process(noteOn);
 * ```
 */
class ProcessorChain {
public:
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     * 
     * @param name Nom de la chaîne
     */
    explicit ProcessorChain(const std::string& name = "Untitled Chain")
        : name_(name)
        , enabled_(true)
        , messagesProcessed_(0) {
        
        Logger::info("ProcessorChain", "Created chain: " + name_);
    }
    
    /**
     * @brief Destructeur
     */
    ~ProcessorChain() {
        Logger::info("ProcessorChain", "Destroyed chain: " + name_);
    }
    
    // Désactiver copie
    ProcessorChain(const ProcessorChain&) = delete;
    ProcessorChain& operator=(const ProcessorChain&) = delete;
    
    // ========================================================================
    // TRAITEMENT
    // ========================================================================
    
    /**
     * @brief Traite un message MIDI à travers la chaîne
     * 
     * @param input Message en entrée
     * @return std::vector<MidiMessage> Messages en sortie
     * 
     * @note Thread-safe
     */
    std::vector<MidiMessage> process(const MidiMessage& input) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Si la chaîne est désactivée, bypass
        if (!enabled_) {
            return {input};
        }
        
        // Si vide, bypass
        if (processors_.empty()) {
            return {input};
        }
        
        // Traiter à travers la chaîne
        std::vector<MidiMessage> current = {input};
        
        for (auto& processor : processors_) {
            if (!processor) {
                continue;
            }
            
            std::vector<MidiMessage> next;
            
            // Traiter chaque message du buffer courant
            for (const auto& msg : current) {
                auto result = processor->process(msg);
                next.insert(next.end(), result.begin(), result.end());
            }
            
            current = std::move(next);
        }
        
        messagesProcessed_ += current.size();
        
        return current;
    }
    
    /**
     * @brief Traite plusieurs messages en batch
     * 
     * @param inputs Messages en entrée
     * @return std::vector<MidiMessage> Messages en sortie
     * 
     * @note Thread-safe
     */
    std::vector<MidiMessage> processBatch(const std::vector<MidiMessage>& inputs) {
        std::vector<MidiMessage> outputs;
        
        for (const auto& input : inputs) {
            auto result = process(input);
            outputs.insert(outputs.end(), result.begin(), result.end());
        }
        
        return outputs;
    }
    
    // ========================================================================
    // GESTION DES PROCESSORS
    // ========================================================================
    
    /**
     * @brief Ajoute un processor à la fin de la chaîne
     * 
     * @param processor Processor à ajouter
     * @return true Si ajouté avec succès
     * 
     * @note Thread-safe
     */
    bool addProcessor(MidiProcessorPtr processor) {
        if (!processor) {
            Logger::error("ProcessorChain", "Cannot add null processor");
            return false;
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        processors_.push_back(processor);
        
        Logger::info("ProcessorChain", "Added processor: " + processor->getName() + 
                    " (total: " + std::to_string(processors_.size()) + ")");
        
        return true;
    }
    
    /**
     * @brief Insère un processor à une position spécifique
     * 
     * @param index Position d'insertion (0-based)
     * @param processor Processor à insérer
     * @return true Si inséré avec succès
     * 
     * @note Thread-safe
     */
    bool insertProcessor(size_t index, MidiProcessorPtr processor) {
        if (!processor) {
            Logger::error("ProcessorChain", "Cannot insert null processor");
            return false;
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (index > processors_.size()) {
            Logger::error("ProcessorChain", "Invalid index: " + std::to_string(index));
            return false;
        }
        
        processors_.insert(processors_.begin() + index, processor);
        
        Logger::info("ProcessorChain", "Inserted processor: " + processor->getName() + 
                    " at index " + std::to_string(index));
        
        return true;
    }
    
    /**
     * @brief Retire un processor par index
     * 
     * @param index Index du processor (0-based)
     * @return true Si retiré avec succès
     * 
     * @note Thread-safe
     */
    bool removeProcessor(size_t index) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (index >= processors_.size()) {
            Logger::error("ProcessorChain", "Invalid index: " + std::to_string(index));
            return false;
        }
        
        std::string name = processors_[index]->getName();
        processors_.erase(processors_.begin() + index);
        
        Logger::info("ProcessorChain", "Removed processor: " + name + 
                    " (remaining: " + std::to_string(processors_.size()) + ")");
        
        return true;
    }
    
    /**
     * @brief Retire tous les processors
     * 
     * @note Thread-safe
     */
    void clearProcessors() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        size_t count = processors_.size();
        processors_.clear();
        
        Logger::info("ProcessorChain", "Cleared all processors (" + 
                    std::to_string(count) + ")");
    }
    
    /**
     * @brief Récupère un processor par index
     * 
     * @param index Index du processor
     * @return MidiProcessorPtr Processor ou nullptr
     * 
     * @note Thread-safe
     */
    MidiProcessorPtr getProcessor(size_t index) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (index >= processors_.size()) {
            return nullptr;
        }
        
        return processors_[index];
    }
    
    /**
     * @brief Récupère tous les processors
     * 
     * @return std::vector<MidiProcessorPtr> Liste des processors
     * 
     * @note Thread-safe
     */
    std::vector<MidiProcessorPtr> getProcessors() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return processors_;
    }
    
    /**
     * @brief Récupère le nombre de processors
     * 
     * @return size_t Nombre de processors
     * 
     * @note Thread-safe
     */
    size_t getProcessorCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return processors_.size();
    }
    
    /**
     * @brief Déplace un processor
     * 
     * @param fromIndex Index source
     * @param toIndex Index destination
     * @return true Si déplacé avec succès
     * 
     * @note Thread-safe
     */
    bool moveProcessor(size_t fromIndex, size_t toIndex) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (fromIndex >= processors_.size() || toIndex >= processors_.size()) {
            Logger::error("ProcessorChain", "Invalid indices");
            return false;
        }
        
        if (fromIndex == toIndex) {
            return true;
        }
        
        auto processor = processors_[fromIndex];
        processors_.erase(processors_.begin() + fromIndex);
        processors_.insert(processors_.begin() + toIndex, processor);
        
        Logger::info("ProcessorChain", "Moved processor from " + 
                    std::to_string(fromIndex) + " to " + std::to_string(toIndex));
        
        return true;
    }
    
    // ========================================================================
    // ÉTAT
    // ========================================================================
    
    /**
     * @brief Active/désactive la chaîne
     * 
     * @param enabled true pour activer
     * 
     * @note Thread-safe
     */
    void setEnabled(bool enabled) {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = enabled;
        
        Logger::info("ProcessorChain", name_ + " " + 
                    (enabled ? "enabled" : "disabled"));
    }
    
    /**
     * @brief Vérifie si la chaîne est active
     * 
     * @note Thread-safe
     */
    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }
    
    /**
     * @brief Réinitialise tous les processors
     * 
     * @note Thread-safe
     */
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (auto& processor : processors_) {
            if (processor) {
                processor->reset();
            }
        }
        
        Logger::info("ProcessorChain", "Reset all processors");
    }
    
    // ========================================================================
    // INFORMATIONS
    // ========================================================================
    
    /**
     * @brief Récupère le nom de la chaîne
     */
    std::string getName() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return name_;
    }
    
    /**
     * @brief Définit le nom de la chaîne
     */
    void setName(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        name_ = name;
    }
    
    /**
     * @brief Récupère les statistiques
     * 
     * @return json Statistiques
     * 
     * @note Thread-safe
     */
    json getStatistics() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        json stats;
        stats["name"] = name_;
        stats["enabled"] = enabled_;
        stats["processor_count"] = processors_.size();
        stats["messages_processed"] = messagesProcessed_.load();
        
        return stats;
    }
    
    // ========================================================================
    // SÉRIALISATION
    // ========================================================================
    
    /**
     * @brief Convertit en JSON
     * 
     * @return json Configuration complète
     * 
     * @note Thread-safe
     */
    json toJson() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        json j;
        j["name"] = name_;
        j["enabled"] = enabled_;
        j["processors"] = json::array();
        
        for (const auto& processor : processors_) {
            if (processor) {
                j["processors"].push_back(processor->toJson());
            }
        }
        
        return j;
    }
    
    /**
     * @brief Configure depuis JSON
     * 
     * @param j Configuration JSON
     * 
     * @note Thread-safe
     * @note Ne recrée pas les processors, juste configure ceux existants
     */
    void fromJson(const json& j) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (j.contains("name")) {
            name_ = j["name"].get<std::string>();
        }
        
        if (j.contains("enabled")) {
            enabled_ = j["enabled"].get<bool>();
        }
        
        if (j.contains("processors") && j["processors"].is_array()) {
            size_t index = 0;
            for (const auto& processorJson : j["processors"]) {
                if (index < processors_.size() && processors_[index]) {
                    processors_[index]->fromJson(processorJson);
                }
                index++;
            }
        }
    }

private:
    /// Nom de la chaîne
    std::string name_;
    
    /// État actif/inactif
    bool enabled_;
    
    /// Liste des processors
    std::vector<MidiProcessorPtr> processors_;
    
    /// Mutex pour thread-safety
    mutable std::mutex mutex_;
    
    /// Statistiques
    std::atomic<uint64_t> messagesProcessed_;
};

/**
 * @brief Alias pour un shared_ptr de ProcessorChain
 */
using ProcessorChainPtr = std::shared_ptr<ProcessorChain>;

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER ProcessorChain.h
// ============================================================================
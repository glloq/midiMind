// ============================================================================
// Fichier: src/midi/processing/ProcessorManager.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Gestionnaire centralisé des processeurs et chaînes MIDI.
//   Point d'entrée principal pour le traitement MIDI.
//
// Responsabilités:
//   - Gérer les chaînes de processors
//   - Router les messages vers les bonnes chaînes
//   - Créer des processors (factory)
//   - Sauvegarder/charger les configurations
//   - Fournir des presets
//
// Thread-safety: OUI
//
// Patterns: Facade Pattern, Factory Pattern
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <functional>

#include "ProcessorChain.h"
#include "MidiProcessor.h"
#include "../MidiMessage.h"
#include "../../core/Logger.h"

// Include tous les processors
#include "basic/TransposeProcessor.h"
#include "basic/VelocityProcessor.h"
#include "basic/ChannelFilterProcessor.h"
#include "basic/NoteFilterProcessor.h"
#include "creative/ArpeggiatorProcessor.h"
#include "creative/DelayProcessor.h"
#include "creative/ChordProcessor.h"
#include "creative/HarmonizerProcessor.h"

namespace midiMind {

/**
 * @class ProcessorManager
 * @brief Gestionnaire centralisé des processeurs MIDI
 * 
 * @details
 * Point d'entrée unique pour tout le traitement MIDI.
 * Gère plusieurs chaînes de processors et route les messages.
 * 
 * Architecture:
 * ```
 * MidiRouter → ProcessorManager → ProcessorChain → Processors → Output
 *                                ↓
 *                         [Chain1, Chain2, ...]
 * ```
 * 
 * Thread-safety: Toutes les méthodes publiques sont thread-safe.
 * 
 * @example Utilisation
 * ```cpp
 * auto manager = std::make_shared<ProcessorManager>();
 * 
 * // Créer une chaîne
 * auto chainId = manager->createChain("Lead Synth");
 * 
 * // Ajouter des processors
 * manager->addProcessorToChain(chainId, 
 *     manager->createProcessor(ProcessorType::TRANSPOSE));
 * 
 * // Traiter un message
 * auto outputs = manager->processMessage(noteOn, chainId);
 * ```
 */
class ProcessorManager {
public:
    // ========================================================================
    // TYPES
    // ========================================================================
    
    /**
     * @brief Callback appelé pour les messages traités
     */
    using MessageOutputCallback = std::function<void(const MidiMessage&, const std::string& chainId)>;
    
    // ========================================================================
    // CONSTRUCTION / DESTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     */
    ProcessorManager();
    
    /**
     * @brief Destructeur
     */
    ~ProcessorManager();
    
    // Désactiver copie
    ProcessorManager(const ProcessorManager&) = delete;
    ProcessorManager& operator=(const ProcessorManager&) = delete;
    
    // ========================================================================
    // TRAITEMENT
    // ========================================================================
    
    /**
     * @brief Traite un message MIDI à travers une chaîne
     * 
     * @param input Message en entrée
     * @param chainId ID de la chaîne
     * @return std::vector<MidiMessage> Messages en sortie
     * 
     * @note Thread-safe
     */
    std::vector<MidiMessage> processMessage(const MidiMessage& input, 
                                           const std::string& chainId);
    
    /**
     * @brief Traite un message MIDI à travers toutes les chaînes actives
     * 
     * @param input Message en entrée
     * @return std::map<std::string, std::vector<MidiMessage>> Messages par chaîne
     * 
     * @note Thread-safe
     */
    std::map<std::string, std::vector<MidiMessage>> processMessageAllChains(
        const MidiMessage& input);
    
    // ========================================================================
    // GESTION DES CHAÎNES
    // ========================================================================
    
    /**
     * @brief Crée une nouvelle chaîne
     * 
     * @param name Nom de la chaîne
     * @return std::string ID de la chaîne créée
     * 
     * @note Thread-safe
     */
    std::string createChain(const std::string& name);
    
    /**
     * @brief Supprime une chaîne
     * 
     * @param chainId ID de la chaîne
     * @return true Si supprimée avec succès
     * 
     * @note Thread-safe
     */
    bool deleteChain(const std::string& chainId);
    
    /**
     * @brief Récupère une chaîne
     * 
     * @param chainId ID de la chaîne
     * @return ProcessorChainPtr Chaîne ou nullptr
     * 
     * @note Thread-safe
     */
    ProcessorChainPtr getChain(const std::string& chainId) const;
    
    /**
     * @brief Liste toutes les chaînes
     * 
     * @return std::vector<std::string> IDs des chaînes
     * 
     * @note Thread-safe
     */
    std::vector<std::string> listChains() const;
    
    /**
     * @brief Renomme une chaîne
     * 
     * @param chainId ID de la chaîne
     * @param newName Nouveau nom
     * @return true Si renommée avec succès
     * 
     * @note Thread-safe
     */
    bool renameChain(const std::string& chainId, const std::string& newName);
    
    // ========================================================================
    // GESTION DES PROCESSORS
    // ========================================================================
    
    /**
     * @brief Crée un processor (factory)
     * 
     * @param type Type de processor
     * @param config Configuration initiale (optionnel)
     * @return MidiProcessorPtr Processor créé
     * 
     * @note Thread-safe
     */
    MidiProcessorPtr createProcessor(ProcessorType type, 
                                     const json& config = json());
    
    /**
     * @brief Ajoute un processor à une chaîne
     * 
     * @param chainId ID de la chaîne
     * @param processor Processor à ajouter
     * @return true Si ajouté avec succès
     * 
     * @note Thread-safe
     */
    bool addProcessorToChain(const std::string& chainId, 
                            MidiProcessorPtr processor);
    
    /**
     * @brief Retire un processor d'une chaîne
     * 
     * @param chainId ID de la chaîne
     * @param processorIndex Index du processor
     * @return true Si retiré avec succès
     * 
     * @note Thread-safe
     */
    bool removeProcessorFromChain(const std::string& chainId, 
                                  size_t processorIndex);
    
    // ========================================================================
    // PRESETS
    // ========================================================================
    
    /**
     * @brief Charge un preset de chaîne
     * 
     * Presets disponibles:
     * - "transpose_up": Transpose +7 demi-tons
     * - "lead_synth": Transpose + Velocity boost
     * - "piano_chords": Chord processor (Major7)
     * - "arp_sequence": Arpégiateur + Delay
     * 
     * @param presetName Nom du preset
     * @return std::string ID de la chaîne créée
     * 
     * @note Thread-safe
     */
    std::string loadPreset(const std::string& presetName);
    
    /**
     * @brief Liste les presets disponibles
     * 
     * @return std::vector<std::string> Noms des presets
     */
    std::vector<std::string> listPresets() const;
    
    // ========================================================================
    // SÉRIALISATION
    // ========================================================================
    
    /**
     * @brief Sauvegarde toutes les chaînes
     * 
     * @param filepath Chemin du fichier
     * @return true Si sauvegardé avec succès
     * 
     * @note Thread-safe
     */
    bool saveToFile(const std::string& filepath) const;
    
    /**
     * @brief Charge toutes les chaînes
     * 
     * @param filepath Chemin du fichier
     * @return true Si chargé avec succès
     * 
     * @note Thread-safe
     */
    bool loadFromFile(const std::string& filepath);
    
    /**
     * @brief Convertit en JSON
     * 
     * @return json Configuration complète
     * 
     * @note Thread-safe
     */
    json toJson() const;
    
    /**
     * @brief Configure depuis JSON
     * 
     * @param j Configuration JSON
     * 
     * @note Thread-safe
     */
    void fromJson(const json& j);
    
    // ========================================================================
    // CALLBACKS
    // ========================================================================
    
    /**
     * @brief Définit le callback de sortie
     */
    void setMessageOutputCallback(MessageOutputCallback callback);
    
    // ========================================================================
    // STATISTIQUES
    // ========================================================================
    
    /**
     * @brief Récupère les statistiques globales
     * 
     * @return json Statistiques
     * 
     * @note Thread-safe
     */
    json getStatistics() const;

private:
    /**
     * @brief Génère un ID unique pour une chaîne
     */
    std::string generateChainId() const;
    
    /**
     * @brief Crée les presets par défaut
     */
    void initializePresets();
    
    /// Chaînes de processors
    std::map<std::string, ProcessorChainPtr> chains_;
    
    /// Mutex pour thread-safety
    mutable std::mutex mutex_;
    
    /// Callback de sortie
    MessageOutputCallback messageOutputCallback_;
    
    /// Compteur pour IDs uniques
    std::atomic<uint32_t> chainIdCounter_;
    
    /// Statistiques
    std::atomic<uint64_t> messagesProcessed_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER ProcessorManager.h
// ============================================================================
// ============================================================================
// Fichier: src/midi/processing/basic/ChannelFilterProcessor.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Processeur de filtrage par canal MIDI.
//   Filtre ou remap les messages selon leur canal.
//
// Thread-safety: Oui
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include "../MidiProcessor.h"
#include <set>
#include <map>

namespace midiMind {

/**
 * @enum ChannelFilterMode
 * @brief Mode de filtrage par canal
 */
enum class ChannelFilterMode {
    WHITELIST,      ///< Ne laisser passer que les canaux listés
    BLACKLIST,      ///< Bloquer les canaux listés
    REMAP           ///< Remapper les canaux
};

/**
 * @class ChannelFilterProcessor
 * @brief Processeur de filtrage par canal
 * 
 * @details
 * Filtre les messages MIDI selon leur canal (1-16).
 * Supporte 3 modes:
 * - Whitelist: Ne laisse passer que certains canaux
 * - Blacklist: Bloque certains canaux
 * - Remap: Change le canal des messages
 * 
 * Paramètres:
 * - mode: Mode de filtrage
 * - channels: Liste des canaux concernés
 * - remap: Map source -> destination (mode REMAP)
 * 
 * Thread-safety: Oui
 * 
 * @example Utilisation
 * ```cpp
 * auto filter = std::make_shared<ChannelFilterProcessor>();
 * filter->setMode(ChannelFilterMode::WHITELIST);
 * filter->addChannel(1); // Ne laisser passer que le canal 1
 * ```
 */
class ChannelFilterProcessor : public MidiProcessor {
public:
    // ========================================================================
    // CONSTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     * 
     * @param mode Mode de filtrage initial
     */
    explicit ChannelFilterProcessor(ChannelFilterMode mode = ChannelFilterMode::WHITELIST)
        : MidiProcessor("ChannelFilter", ProcessorType::CHANNEL_FILTER)
        , mode_(mode) {
        
        parameters_["mode"] = static_cast<int>(mode);
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
        
        // Ne traiter que les messages avec canal
        if (!input.hasChannel()) {
            return {input};
        }
        
        uint8_t channel = input.getChannel();
        
        switch (mode_) {
            case ChannelFilterMode::WHITELIST:
                // Ne laisser passer que si dans la liste
                if (channels_.find(channel) == channels_.end()) {
                    return {}; // Filtré
                }
                return {input};
                
            case ChannelFilterMode::BLACKLIST:
                // Bloquer si dans la liste
                if (channels_.find(channel) != channels_.end()) {
                    return {}; // Filtré
                }
                return {input};
                
            case ChannelFilterMode::REMAP: {
                // Remapper le canal
                auto it = remapTable_.find(channel);
                if (it != remapTable_.end()) {
                    MidiMessage output = input;
                    output.setChannel(it->second);
                    return {output};
                }
                return {input}; // Pas de remap défini
            }
        }
        
        return {input};
    }
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    /**
     * @brief Définit le mode
     */
    void setMode(ChannelFilterMode mode) {
        mode_ = mode;
        parameters_["mode"] = static_cast<int>(mode);
    }
    
    /**
     * @brief Récupère le mode
     */
    ChannelFilterMode getMode() const {
        return mode_;
    }
    
    /**
     * @brief Ajoute un canal à la liste
     * 
     * @param channel Canal MIDI (1-16)
     */
    void addChannel(uint8_t channel) {
        if (channel >= 1 && channel <= 16) {
            channels_.insert(channel);
            updateChannelsParameter();
        }
    }
    
    /**
     * @brief Retire un canal de la liste
     * 
     * @param channel Canal MIDI (1-16)
     */
    void removeChannel(uint8_t channel) {
        channels_.erase(channel);
        updateChannelsParameter();
    }
    
    /**
     * @brief Efface tous les canaux
     */
    void clearChannels() {
        channels_.clear();
        updateChannelsParameter();
    }
    
    /**
     * @brief Définit la liste des canaux
     * 
     * @param channels Set de canaux
     */
    void setChannels(const std::set<uint8_t>& channels) {
        channels_ = channels;
        updateChannelsParameter();
    }
    
    /**
     * @brief Récupère la liste des canaux
     */
    const std::set<uint8_t>& getChannels() const {
        return channels_;
    }
    
    /**
     * @brief Définit une règle de remap
     * 
     * @param from Canal source (1-16)
     * @param to Canal destination (1-16)
     */
    void setRemap(uint8_t from, uint8_t to) {
        if (from >= 1 && from <= 16 && to >= 1 && to <= 16) {
            remapTable_[from] = to;
            updateRemapParameter();
        }
    }
    
    /**
     * @brief Retire une règle de remap
     * 
     * @param from Canal source
     */
    void removeRemap(uint8_t from) {
        remapTable_.erase(from);
        updateRemapParameter();
    }
    
    /**
     * @brief Efface toutes les règles de remap
     */
    void clearRemap() {
        remapTable_.clear();
        updateRemapParameter();
    }
    
    /**
     * @brief Définit un paramètre
     */
    bool setParameter(const std::string& name, const json& value) override {
        if (name == "mode") {
            setMode(static_cast<ChannelFilterMode>(value.get<int>()));
            return true;
        } else if (name == "channels") {
            channels_.clear();
            if (value.is_array()) {
                for (const auto& ch : value) {
                    addChannel(ch.get<uint8_t>());
                }
            }
            return true;
        } else if (name == "remap") {
            remapTable_.clear();
            if (value.is_object()) {
                for (auto& [key, val] : value.items()) {
                    uint8_t from = std::stoi(key);
                    uint8_t to = val.get<uint8_t>();
                    setRemap(from, to);
                }
            }
            return true;
        }
        
        return MidiProcessor::setParameter(name, value);
    }

private:
    /**
     * @brief Met à jour le paramètre channels dans le JSON
     */
    void updateChannelsParameter() {
        json channelsArray = json::array();
        for (uint8_t ch : channels_) {
            channelsArray.push_back(ch);
        }
        parameters_["channels"] = channelsArray;
    }
    
    /**
     * @brief Met à jour le paramètre remap dans le JSON
     */
    void updateRemapParameter() {
        json remapObj = json::object();
        for (const auto& [from, to] : remapTable_) {
            remapObj[std::to_string(from)] = to;
        }
        parameters_["remap"] = remapObj;
    }
    
    /// Mode de filtrage
    ChannelFilterMode mode_;
    
    /// Canaux concernés (pour WHITELIST et BLACKLIST)
    std::set<uint8_t> channels_;
    
    /// Table de remapping (pour REMAP)
    std::map<uint8_t, uint8_t> remapTable_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER ChannelFilterProcessor.h
// ============================================================================
// ============================================================================
// Fichier: src/midi/processing/basic/NoteFilterProcessor.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Processeur de filtrage par note MIDI.
//   Filtre les notes selon des critères (range, liste, etc.).
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

namespace midiMind {

/**
 * @enum NoteFilterMode
 * @brief Mode de filtrage par note
 */
enum class NoteFilterMode {
    RANGE,          ///< Filtrer par plage (min-max)
    WHITELIST,      ///< Ne laisser passer que certaines notes
    BLACKLIST       ///< Bloquer certaines notes
};

/**
 * @class NoteFilterProcessor
 * @brief Processeur de filtrage par note
 * 
 * @details
 * Filtre les messages Note On/Off selon la hauteur de note.
 * Supporte 3 modes:
 * - Range: Ne laisse passer qu'une plage de notes
 * - Whitelist: Ne laisse passer que certaines notes
 * - Blacklist: Bloque certaines notes
 * 
 * Paramètres:
 * - mode: Mode de filtrage
 * - min_note: Note minimale (mode RANGE)
 * - max_note: Note maximale (mode RANGE)
 * - notes: Liste des notes (mode WHITELIST/BLACKLIST)
 * 
 * Thread-safety: Oui
 * 
 * @example Utilisation
 * ```cpp
 * auto filter = std::make_shared<NoteFilterProcessor>();
 * filter->setMode(NoteFilterMode::RANGE);
 * filter->setRange(60, 72); // Do central à Do aigu
 * ```
 */
class NoteFilterProcessor : public MidiProcessor {
public:
    // ========================================================================
    // CONSTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur
     * 
     * @param mode Mode de filtrage initial
     */
    explicit NoteFilterProcessor(NoteFilterMode mode = NoteFilterMode::RANGE)
        : MidiProcessor("NoteFilter", ProcessorType::NOTE_FILTER)
        , mode_(mode)
        , minNote_(0)
        , maxNote_(127) {
        
        parameters_["mode"] = static_cast<int>(mode);
        parameters_["min_note"] = minNote_;
        parameters_["max_note"] = maxNote_;
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
        
        uint8_t note = input.getNote();
        
        switch (mode_) {
            case NoteFilterMode::RANGE:
                // Vérifier si dans la plage
                if (note < minNote_ || note > maxNote_) {
                    return {}; // Filtré
                }
                return {input};
                
            case NoteFilterMode::WHITELIST:
                // Ne laisser passer que si dans la liste
                if (notes_.find(note) == notes_.end()) {
                    return {}; // Filtré
                }
                return {input};
                
            case NoteFilterMode::BLACKLIST:
                // Bloquer si dans la liste
                if (notes_.find(note) != notes_.end()) {
                    return {}; // Filtré
                }
                return {input};
        }
        
        return {input};
    }
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    /**
     * @brief Définit le mode
     */
    void setMode(NoteFilterMode mode) {
        mode_ = mode;
        parameters_["mode"] = static_cast<int>(mode);
    }
    
    /**
     * @brief Récupère le mode
     */
    NoteFilterMode getMode() const {
        return mode_;
    }
    
    /**
     * @brief Définit la plage de notes (mode RANGE)
     * 
     * @param minNote Note minimale (0-127)
     * @param maxNote Note maximale (0-127)
     */
    void setRange(uint8_t minNote, uint8_t maxNote) {
        if (minNote <= maxNote) {
            minNote_ = minNote;
            maxNote_ = maxNote;
            parameters_["min_note"] = minNote_;
            parameters_["max_note"] = maxNote_;
        }
    }
    
    /**
     * @brief Récupère la plage de notes
     */
    std::pair<uint8_t, uint8_t> getRange() const {
        return {minNote_, maxNote_};
    }
    
    /**
     * @brief Ajoute une note à la liste
     * 
     * @param note Note MIDI (0-127)
     */
    void addNote(uint8_t note) {
        if (note <= 127) {
            notes_.insert(note);
            updateNotesParameter();
        }
    }
    
    /**
     * @brief Retire une note de la liste
     * 
     * @param note Note MIDI (0-127)
     */
    void removeNote(uint8_t note) {
        notes_.erase(note);
        updateNotesParameter();
    }
    
    /**
     * @brief Efface toutes les notes
     */
    void clearNotes() {
        notes_.clear();
        updateNotesParameter();
    }
    
    /**
     * @brief Définit la liste des notes
     * 
     * @param notes Set de notes
     */
    void setNotes(const std::set<uint8_t>& notes) {
        notes_ = notes;
        updateNotesParameter();
    }
    
    /**
     * @brief Récupère la liste des notes
     */
    const std::set<uint8_t>& getNotes() const {
        return notes_;
    }
    
    /**
     * @brief Définit un paramètre
     */
    bool setParameter(const std::string& name, const json& value) override {
        if (name == "mode") {
            setMode(static_cast<NoteFilterMode>(value.get<int>()));
            return true;
        } else if (name == "min_note") {
            minNote_ = value.get<uint8_t>();
            parameters_["min_note"] = minNote_;
            return true;
        } else if (name == "max_note") {
            maxNote_ = value.get<uint8_t>();
            parameters_["max_note"] = maxNote_;
            return true;
        } else if (name == "notes") {
            notes_.clear();
            if (value.is_array()) {
                for (const auto& note : value) {
                    addNote(note.get<uint8_t>());
                }
            }
            return true;
        }
        
        return MidiProcessor::setParameter(name, value);
    }

private:
    /**
     * @brief Met à jour le paramètre notes dans le JSON
     */
    void updateNotesParameter() {
        json notesArray = json::array();
        for (uint8_t note : notes_) {
            notesArray.push_back(note);
        }
        parameters_["notes"] = notesArray;
    }
    
    /// Mode de filtrage
    NoteFilterMode mode_;
    
    /// Note minimale (mode RANGE)
    uint8_t minNote_;
    
    /// Note maximale (mode RANGE)
    uint8_t maxNote_;
    
    /// Notes concernées (mode WHITELIST/BLACKLIST)
    std::set<uint8_t> notes_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER NoteFilterProcessor.h
// ============================================================================
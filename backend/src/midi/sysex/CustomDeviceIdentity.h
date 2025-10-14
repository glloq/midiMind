// ============================================================================
// Fichier: src/midi/sysex/CustomDeviceIdentity.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Structures pour les informations Custom SysEx (protocole 0x7D)
//   Bloc 1 - Identification complète des instruments DIY
//
// Auteur: MidiMind Team
// Date: 2025-10-06
// Version: 3.0.0
// ============================================================================

#pragma once

#include <string>
#include <array>
#include <cstdint>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

/**
 * @struct CustomDeviceIdentity
 * @brief Identité complète d'un instrument DIY (Bloc 1)
 * 
 * @details
 * Structure retournée par le Bloc 1 du protocole Custom SysEx.
 * Contient toutes les informations d'identification et capacités
 * d'un instrument DIY.
 * 
 * Format du message Bloc 1:
 * F0 7D <DeviceID> 01 01 
 * <UniqueID[4]>           // 28-bit unique ID
 * <Name...> 00            // Nom null-terminated
 * <Type>                  // Type d'instrument
 * <FirstNote>             // Première note MIDI
 * <NoteCount>             // Nombre de notes
 * <MaxPoly>               // Polyphonie max
 * <TuningMode>            // Mode d'accordage
 * <DelayLSB> <DelayMSB>   // Délai de réponse (ms)
 * <FwV1> <FwV2> <FwV3> <FwV4>  // Version firmware
 * <Flags>                 // Capacités (bitfield)
 * <Programs>              // Nombre de programmes
 * F7
 */
struct CustomDeviceIdentity {
    // Bloc 1 - Identification
    uint32_t uniqueId;                      ///< ID unique 28-bit
    std::string name;                       ///< Nom de l'instrument
    uint8_t type;                           ///< Type GM étendu
    uint8_t firstNote;                      ///< Première note
    uint8_t noteCount;                      ///< Nombre de notes
    uint8_t maxPolyphony;                   ///< Polyphonie max
    uint8_t tuningMode;                     ///< Mode d'accordage
    uint16_t responseDelay;                 ///< Délai de réponse (ms)
    std::array<uint8_t, 4> firmwareVersion; ///< Version [Major, Minor, Patch, Build]
    uint8_t flags;                          ///< Capacités (bitfield)
    uint8_t programCount;                   ///< Nombre de presets
    
    /**
     * @brief Constructeur par défaut
     */
    CustomDeviceIdentity()
        : uniqueId(0)
        , type(0)
        , firstNote(0)
        , noteCount(0)
        , maxPolyphony(0)
        , tuningMode(0)
        , responseDelay(0)
        , firmwareVersion({0, 0, 0, 0})
        , flags(0)
        , programCount(0) {}
    
    /**
     * @brief Calcule la dernière note jouable
     */
    uint8_t getLastNote() const {
        if (noteCount == 0) return firstNote;
        return firstNote + noteCount - 1;
    }
    
    /**
     * @brief Vérifie si l'instrument supporte la vélocité
     */
    bool hasVelocity() const {
        return (flags & 0x01) != 0;
    }
    
    /**
     * @brief Vérifie si l'instrument supporte l'aftertouch
     */
    bool hasAftertouch() const {
        return (flags & 0x02) != 0;
    }
    
    /**
     * @brief Vérifie si l'instrument supporte le breath controller
     */
    bool hasBreath() const {
        return (flags & 0x04) != 0;
    }
    
    /**
     * @brief Vérifie si l'instrument supporte le pitch bend
     */
    bool hasPitchBend() const {
        return (flags & 0x08) != 0;
    }
    
    /**
     * @brief Vérifie si l'instrument supporte la modulation wheel
     */
    bool hasModulation() const {
        return (flags & 0x10) != 0;
    }
    
    /**
     * @brief Vérifie si l'instrument supporte l'expression pedal
     */
    bool hasExpression() const {
        return (flags & 0x20) != 0;
    }
    
    /**
     * @brief Vérifie si l'instrument supporte le sustain pedal
     */
    bool hasSustain() const {
        return (flags & 0x40) != 0;
    }
    
    /**
     * @brief Vérifie si l'instrument supporte les program changes
     */
    bool hasProgramChange() const {
        return (flags & 0x80) != 0;
    }
    
    /**
     * @brief Formate la version du firmware en string
     */
    std::string getFirmwareString() const {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d.%d.%d.%d",
                firmwareVersion[0], firmwareVersion[1],
                firmwareVersion[2], firmwareVersion[3]);
        return std::string(buf);
    }
    
    /**
     * @brief Obtient le nom de la catégorie de l'instrument
     */
    std::string getTypeCategory() const {
        if (type >= 0x80) return "DIY";
        if (type >= 0x78) return "Sound Effects";
        if (type >= 0x70) return "Percussive";
        if (type >= 0x68) return "Ethnic";
        if (type >= 0x60) return "Synth Effects";
        if (type >= 0x58) return "Synth Pad";
        if (type >= 0x50) return "Synth Lead";
        if (type >= 0x48) return "Pipe";
        if (type >= 0x40) return "Reed";
        if (type >= 0x38) return "Brass";
        if (type >= 0x30) return "Ensemble";
        if (type >= 0x28) return "Strings";
        if (type >= 0x20) return "Bass";
        if (type >= 0x18) return "Guitar";
        if (type >= 0x10) return "Organ";
        if (type >= 0x08) return "Chromatic Percussion";
        return "Piano";
    }
    
    /**
     * @brief Obtient le type de polyphonie
     */
    std::string getPolyphonyType() const {
        if (maxPolyphony == 0) return "monophonic";
        if (maxPolyphony == 1) return "monophonic (legato)";
        return "polyphonic (" + std::to_string(maxPolyphony) + " voices)";
    }
    
    /**
     * @brief Obtient le nom du mode de tuning
     */
    std::string getTuningModeName() const {
        switch (tuningMode) {
            case 0x00: return "chromatic";
            case 0x01: return "diatonic";
            case 0x02: return "pentatonic";
            case 0x03: return "blues";
            case 0x04: return "whole-tone";
            case 0x05: return "octatonic";
            case 0x08: return "mono";
            case 0x09: return "poly";
            case 0x0A: return "drone";
            case 0x0B: return "cluster";
            default: return "unknown";
        }
    }
    
    /**
     * @brief Obtient le nom d'une note MIDI
     */
    static std::string getNoteName(uint8_t note) {
        const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", 
                                   "F#", "G", "G#", "A", "A#", "B"};
        int octave = (note / 12) - 1;
        int noteIndex = note % 12;
        
        char buf[8];
        snprintf(buf, sizeof(buf), "%s%d", noteNames[noteIndex], octave);
        return std::string(buf);
    }
    
    /**
     * @brief Convertit en JSON
     */
    json toJson() const {
        json j;
        
        // Informations de base
        j["uniqueId"] = "0x" + std::to_string(uniqueId);
        j["name"] = name;
        
        // Type
        j["type"]["code"] = type;
        j["type"]["category"] = getTypeCategory();
        
        // Range
        j["range"]["firstNote"] = firstNote;
        j["range"]["lastNote"] = getLastNote();
        j["range"]["noteCount"] = noteCount;
        j["range"]["firstNoteName"] = getNoteName(firstNote);
        j["range"]["lastNoteName"] = getNoteName(getLastNote());
        
        // Polyphonie
        j["polyphony"]["maxVoices"] = maxPolyphony;
        j["polyphony"]["type"] = getPolyphonyType();
        
        // Tuning
        j["tuning"]["mode"] = getTuningModeName();
        j["tuning"]["code"] = tuningMode;
        
        // Latence
        j["latency"]["responseDelay"] = responseDelay;
        j["latency"]["unit"] = "ms";
        
        // Firmware
        j["firmware"]["version"] = getFirmwareString();
        j["firmware"]["major"] = firmwareVersion[0];
        j["firmware"]["minor"] = firmwareVersion[1];
        j["firmware"]["patch"] = firmwareVersion[2];
        j["firmware"]["build"] = firmwareVersion[3];
        
        // Capacités
        j["capabilities"]["velocity"] = hasVelocity();
        j["capabilities"]["aftertouch"] = hasAftertouch();
        j["capabilities"]["breath"] = hasBreath();
        j["capabilities"]["pitchBend"] = hasPitchBend();
        j["capabilities"]["modulation"] = hasModulation();
        j["capabilities"]["expression"] = hasExpression();
        j["capabilities"]["sustain"] = hasSustain();
        j["capabilities"]["programChange"] = hasProgramChange();
        
        // Programmes
        j["programs"]["count"] = programCount;
        if (programCount > 0) {
            json programs = json::array();
            for (uint8_t i = 0; i < programCount; ++i) {
                programs.push_back(i);
            }
            j["programs"]["list"] = programs;
        }
        
        return j;
    }
    
    /**
     * @brief Convertit en string descriptif
     */
    std::string toString() const {
        return name + " (" + getTypeCategory() + ") - " +
               getNoteName(firstNote) + " to " + getNoteName(getLastNote()) +
               " - " + getPolyphonyType() + " - FW: " + getFirmwareString();
    }
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER CustomDeviceIdentity.h
// ============================================================================

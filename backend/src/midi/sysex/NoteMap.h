// ============================================================================
// Fichier: src/midi/sysex/NoteMap.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Structure pour la Note Map Custom SysEx (protocole 0x7D)
//   Bloc 2 - Bitmap 128-bit des notes jouables
//
// Auteur: MidiMind Team
// Date: 2025-10-06
// Version: 3.0.0
// ============================================================================

#pragma once

#include <array>
#include <vector>
#include <string>
#include <cstdint>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

/**
 * @struct NoteMap
 * @brief Carte des notes jouables d'un instrument (Bloc 2)
 * 
 * @details
 * Structure retournée par le Bloc 2 du protocole Custom SysEx.
 * Contient un bitmap 128-bit indiquant quelles notes MIDI (0-127)
 * sont jouables sur l'instrument.
 * 
 * Format du message Bloc 2:
 * F0 7D <DeviceID> 02 01 
 * <Bitmap[16]>         // 128 bits = 16 bytes (notes 0-127)
 * <Reserved[2]>        // 2 bytes réservés (0x00)
 * F7
 * 
 * Encodage du bitmap:
 * - 1 bit par note MIDI (0-127)
 * - Bit à 1 : Note jouable
 * - Bit à 0 : Note non jouable
 * - Byte 0 : Notes 0-6 (bit 0 = note 0, bit 6 = note 6)
 * - Byte 1 : Notes 7-13
 * - ...
 * - Byte 18 : Notes 119-127 (bits 0-1 utilisés, bits 2-6 inutilisés)
 */
struct NoteMap {
    std::array<uint8_t, 19> bitmap;  ///< 128 bits + padding (19 bytes pour 128 notes encodées en 7-bit)
    
    /**
     * @brief Constructeur par défaut (aucune note jouable)
     */
    NoteMap() {
        bitmap.fill(0);
    }
    
    /**
     * @brief Vérifie si une note est jouable
     * 
     * @param note Numéro de note MIDI (0-127)
     * @return true si la note est jouable
     */
    bool isNotePlayable(uint8_t note) const {
        if (note > 127) return false;
        
        uint8_t byteIndex = note / 7;
        uint8_t bitIndex = note % 7;
        
        return (bitmap[byteIndex] & (1 << bitIndex)) != 0;
    }
    
    /**
     * @brief Active/désactive une note
     * 
     * @param note Numéro de note MIDI (0-127)
     * @param playable true pour activer, false pour désactiver
     */
    void setNotePlayable(uint8_t note, bool playable) {
        if (note > 127) return;
        
        uint8_t byteIndex = note / 7;
        uint8_t bitIndex = note % 7;
        
        if (playable) {
            bitmap[byteIndex] |= (1 << bitIndex);
        } else {
            bitmap[byteIndex] &= ~(1 << bitIndex);
        }
    }
    
    /**
     * @brief Active une plage de notes continues
     * 
     * @param firstNote Première note de la plage (0-127)
     * @param lastNote Dernière note de la plage (0-127)
     */
    void setNoteRange(uint8_t firstNote, uint8_t lastNote) {
        if (firstNote > 127 || lastNote > 127 || firstNote > lastNote) return;
        
        for (uint8_t note = firstNote; note <= lastNote; ++note) {
            setNotePlayable(note, true);
        }
    }
    
    /**
     * @brief Obtient la liste de toutes les notes jouables
     * 
     * @return std::vector<uint8_t> Liste des numéros de notes
     */
    std::vector<uint8_t> getPlayableNotes() const {
        std::vector<uint8_t> notes;
        notes.reserve(128);
        
        for (uint8_t note = 0; note <= 127; ++note) {
            if (isNotePlayable(note)) {
                notes.push_back(note);
            }
        }
        
        return notes;
    }
    
    /**
     * @brief Compte le nombre de notes jouables
     * 
     * @return uint8_t Nombre de notes actives
     */
    uint8_t countPlayableNotes() const {
        uint8_t count = 0;
        
        for (uint8_t byte : bitmap) {
            // Population count (nombre de bits à 1)
            uint8_t n = byte;
            while (n) {
                count += n & 1;
                n >>= 1;
            }
        }
        
        return count;
    }
    
    /**
     * @brief Obtient les plages continues de notes
     * 
     * @return std::vector<std::pair<uint8_t, uint8_t>> Liste des plages (début, fin)
     */
    std::vector<std::pair<uint8_t, uint8_t>> getNoteRanges() const {
        std::vector<std::pair<uint8_t, uint8_t>> ranges;
        
        bool inRange = false;
        uint8_t rangeStart = 0;
        
        for (uint8_t note = 0; note <= 127; ++note) {
            bool playable = isNotePlayable(note);
            
            if (playable && !inRange) {
                // Début d'une nouvelle plage
                rangeStart = note;
                inRange = true;
            } else if (!playable && inRange) {
                // Fin de la plage actuelle
                ranges.push_back({rangeStart, note - 1});
                inRange = false;
            }
        }
        
        // Si on termine dans une plage
        if (inRange) {
            ranges.push_back({rangeStart, 127});
        }
        
        return ranges;
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
     * @brief Convertit le bitmap en string hexadécimal
     */
    std::string bitmapToHex() const {
        std::string hex;
        hex.reserve(bitmap.size() * 2);
        
        for (uint8_t byte : bitmap) {
            char buf[4];
            snprintf(buf, sizeof(buf), "%02X", byte);
            hex += buf;
        }
        
        return hex;
    }
    
    /**
     * @brief Convertit en JSON
     */
    json toJson() const {
        json j;
        
        // Statistiques
        j["totalNotes"] = countPlayableNotes();
        
        // Liste des notes jouables
        auto playableNotes = getPlayableNotes();
        j["playableNotes"] = playableNotes;
        
        // Noms des notes
        json noteNames = json::array();
        for (uint8_t note : playableNotes) {
            noteNames.push_back(getNoteName(note));
        }
        j["noteNames"] = noteNames;
        
        // Plages de notes
        auto ranges = getNoteRanges();
        json rangesJson = json::array();
        for (const auto& range : ranges) {
            json rangeObj;
            rangeObj["start"] = range.first;
            rangeObj["end"] = range.second;
            rangeObj["startName"] = getNoteName(range.first);
            rangeObj["endName"] = getNoteName(range.second);
            
            // Calculer le nombre d'octaves
            int octaves = (range.second - range.first + 1) / 12;
            if (octaves > 0) {
                rangeObj["octaves"] = octaves;
            }
            
            rangesJson.push_back(rangeObj);
        }
        j["ranges"] = rangesJson;
        
        // Bitmap en hex
        j["bitmap"] = bitmapToHex();
        
        return j;
    }
    
    /**
     * @brief Convertit en string descriptif
     */
    std::string toString() const {
        auto ranges = getNoteRanges();
        
        if (ranges.empty()) {
            return "No playable notes";
        }
        
        std::string result = std::to_string(countPlayableNotes()) + " notes: ";
        
        for (size_t i = 0; i < ranges.size(); ++i) {
            if (i > 0) result += ", ";
            result += getNoteName(ranges[i].first) + "-" + getNoteName(ranges[i].second);
        }
        
        return result;
    }
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER NoteMap.h
// ============================================================================

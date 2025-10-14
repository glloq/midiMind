// ============================================================================
// Fichier: src/midi/sysex/CCCapabilities.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Structure pour les CC Supportés Custom SysEx (protocole 0x7D)
//   Bloc 3 - Liste des Control Change supportés
//
// Auteur: MidiMind Team
// Date: 2025-10-06
// Version: 3.0.0
// ============================================================================

#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

/**
 * @struct CCCapabilities
 * @brief Liste des Control Change supportés (Bloc 3)
 * 
 * @details
 * Structure retournée par le Bloc 3 du protocole Custom SysEx.
 * Contient la liste des numéros CC que l'instrument peut traiter.
 * 
 * Format du message Bloc 3:
 * F0 7D <DeviceID> 03 02
 * <CCCount>            // Nombre de CC supportés (1-128)
 * <CC1> <CC2> ... <CCn>  // Liste des numéros CC (0-127)
 * F7
 */
struct CCCapabilities {
    std::vector<uint8_t> supportedCC;  ///< Liste des CC supportés (0-127)
    
    /**
     * @brief Constructeur par défaut
     */
    CCCapabilities() = default;
    
    /**
     * @brief Vérifie si un CC est supporté
     * 
     * @param ccNumber Numéro de CC (0-127)
     * @return true si le CC est supporté
     */
    bool isSupported(uint8_t ccNumber) const {
        if (ccNumber > 127) return false;
        
        for (uint8_t cc : supportedCC) {
            if (cc == ccNumber) return true;
        }
        
        return false;
    }
    
    /**
     * @brief Ajoute un CC à la liste
     * 
     * @param ccNumber Numéro de CC (0-127)
     */
    void addCC(uint8_t ccNumber) {
        if (ccNumber > 127) return;
        
        // Éviter les doublons
        if (!isSupported(ccNumber)) {
            supportedCC.push_back(ccNumber);
        }
    }
    
    /**
     * @brief Obtient le nombre de CC supportés
     */
    size_t count() const {
        return supportedCC.size();
    }
    
    /**
     * @brief Obtient le nom d'un CC
     */
    static std::string getCCName(uint8_t ccNumber) {
        switch (ccNumber) {
            case 1: return "Modulation Wheel";
            case 2: return "Breath Controller";
            case 4: return "Foot Controller";
            case 5: return "Portamento Time";
            case 7: return "Channel Volume";
            case 8: return "Balance";
            case 10: return "Pan";
            case 11: return "Expression";
            case 64: return "Sustain Pedal";
            case 65: return "Portamento";
            case 66: return "Sostenuto";
            case 67: return "Soft Pedal";
            case 68: return "Legato Footswitch";
            case 69: return "Hold 2";
            case 70: return "Sound Controller 1 (Brightness)";
            case 71: return "Sound Controller 2 (Timbre)";
            case 72: return "Sound Controller 3 (Release Time)";
            case 73: return "Sound Controller 4 (Attack Time)";
            case 74: return "Sound Controller 5 (Brightness)";
            case 75: return "Sound Controller 6";
            case 76: return "Sound Controller 7";
            case 77: return "Sound Controller 8";
            case 78: return "Sound Controller 9";
            case 79: return "Sound Controller 10";
            case 84: return "Portamento Control";
            case 91: return "Effects 1 Depth (Reverb)";
            case 92: return "Effects 2 Depth (Tremolo)";
            case 93: return "Effects 3 Depth (Chorus)";
            case 94: return "Effects 4 Depth (Detune)";
            case 95: return "Effects 5 Depth (Phaser)";
            default:
                char buf[32];
                snprintf(buf, sizeof(buf), "CC %d", ccNumber);
                return std::string(buf);
        }
    }
    
    /**
     * @brief Convertit en JSON
     */
    json toJson() const {
        json j;
        
        j["count"] = supportedCC.size();
        j["ccNumbers"] = supportedCC;
        
        // Ajouter les noms
        json ccList = json::array();
        for (uint8_t cc : supportedCC) {
            json ccInfo;
            ccInfo["number"] = cc;
            ccInfo["name"] = getCCName(cc);
            ccList.push_back(ccInfo);
        }
        j["controllers"] = ccList;
        
        return j;
    }
    
    /**
     * @brief Convertit en string descriptif
     */
    std::string toString() const {
        if (supportedCC.empty()) {
            return "No CC supported";
        }
        
        std::string result = std::to_string(supportedCC.size()) + " CC supported: ";
        
        for (size_t i = 0; i < supportedCC.size() && i < 5; ++i) {
            if (i > 0) result += ", ";
            result += std::to_string(supportedCC[i]);
        }
        
        if (supportedCC.size() > 5) {
            result += "...";
        }
        
        return result;
    }
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER CCCapabilities.h
// ============================================================================

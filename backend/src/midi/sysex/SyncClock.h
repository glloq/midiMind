// ============================================================================
// Fichier: src/midi/sysex/SyncClock.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Structure pour Sync & Clock Custom SysEx (protocole 0x7D)
//   Bloc 8 - Capacités de synchronisation
//
// Auteur: MidiMind Team
// Date: 2025-10-06
// Version: 3.0.0
// ============================================================================

#pragma once

#include <string>
#include <cstdint>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

/**
 * @struct SyncClock
 * @brief Capacités de synchronisation (Bloc 8)
 * 
 * @details
 * Structure retournée par le Bloc 8 du protocole Custom SysEx.
 * Indique les capacités de synchronisation MIDI de l'instrument.
 * 
 * Format du message Bloc 8:
 * F0 7D <DeviceID> 08 03
 * <ClockSupport>       // Support MIDI Clock (0=No, 1=Yes)
 * <MTCSupport>         // Support MTC (0=No, 1=Yes)
 * <InternalTempo>      // Tempo interne (0=No, 1-250 BPM)
 * <Reserved[8]>        // 8 bytes réservés
 * F7
 */
struct SyncClock {
    bool clockSupport;      ///< Support MIDI Clock
    bool mtcSupport;        ///< Support MIDI Time Code (MTC)
    uint8_t internalTempo;  ///< Tempo interne (0=désactivé, 1-250 BPM)
    
    /**
     * @brief Constructeur par défaut
     */
    SyncClock()
        : clockSupport(false)
        , mtcSupport(false)
        , internalTempo(0) {}
    
    /**
     * @brief Vérifie si l'instrument supporte la synchronisation
     */
    bool hasSync() const {
        return clockSupport || mtcSupport || hasInternalClock();
    }
    
    /**
     * @brief Vérifie si l'instrument a une horloge interne
     */
    bool hasInternalClock() const {
        return internalTempo > 0;
    }
    
    /**
     * @brief Vérifie si le tempo est dans une plage valide
     */
    bool isTempoValid() const {
        return internalTempo == 0 || (internalTempo >= 1 && internalTempo <= 250);
    }
    
    /**
     * @brief Obtient le mode de synchronisation principal
     */
    std::string getSyncMode() const {
        if (hasInternalClock()) {
            return "Internal Clock";
        } else if (clockSupport && mtcSupport) {
            return "MIDI Clock + MTC";
        } else if (clockSupport) {
            return "MIDI Clock";
        } else if (mtcSupport) {
            return "MTC";
        } else {
            return "None";
        }
    }
    
    /**
     * @brief Obtient la description du tempo interne
     */
    std::string getTempoDescription() const {
        if (!hasInternalClock()) {
            return "No internal clock";
        }
        
        char buf[32];
        snprintf(buf, sizeof(buf), "%d BPM", internalTempo);
        return std::string(buf);
    }
    
    /**
     * @brief Convertit en JSON
     */
    json toJson() const {
        json j;
        
        j["hasSync"] = hasSync();
        j["syncMode"] = getSyncMode();
        
        j["capabilities"]["midiClock"] = clockSupport;
        j["capabilities"]["mtc"] = mtcSupport;
        j["capabilities"]["internalClock"] = hasInternalClock();
        
        if (hasInternalClock()) {
            j["internalClock"]["enabled"] = true;
            j["internalClock"]["tempo"] = internalTempo;
            j["internalClock"]["description"] = getTempoDescription();
        } else {
            j["internalClock"]["enabled"] = false;
        }
        
        // Recommandations d'usage
        json recommendations;
        if (clockSupport) {
            recommendations.push_back("Can sync to external MIDI Clock");
        }
        if (mtcSupport) {
            recommendations.push_back("Can sync to MIDI Time Code");
        }
        if (hasInternalClock()) {
            recommendations.push_back("Has internal tempo generator");
        }
        j["recommendations"] = recommendations;
        
        return j;
    }
    
    /**
     * @brief Convertit en string descriptif
     */
    std::string toString() const {
        if (!hasSync()) {
            return "No sync capabilities";
        }
        
        std::string result = getSyncMode();
        
        if (hasInternalClock()) {
            result += " @ " + getTempoDescription();
        }
        
        return result;
    }
    
    /**
     * @brief Obtient des conseils pour la configuration
     */
    std::vector<std::string> getConfigTips() const {
        std::vector<std::string> tips;
        
        if (clockSupport && !hasInternalClock()) {
            tips.push_back("Connect to a DAW or sequencer for MIDI Clock sync");
        }
        
        if (mtcSupport) {
            tips.push_back("Can sync with video/audio timecode");
        }
        
        if (hasInternalClock()) {
            tips.push_back("Can operate as standalone clock source");
            tips.push_back("Set tempo via MIDI CC or SysEx");
        }
        
        if (!hasSync()) {
            tips.push_back("No sync required - free-running mode only");
        }
        
        return tips;
    }
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER SyncClock.h
// ============================================================================

// ============================================================================
// Fichier: src/midi/sysex/AirCapabilities.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Structure pour les Capacités Air Custom SysEx (protocole 0x7D)
//   Bloc 4 - Détail des capacités de contrôle par souffle/air
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
 * @enum BreathType
 * @brief Type de capteur de souffle
 */
enum class BreathType : uint8_t {
    NONE = 0,       ///< Pas de capteur de souffle
    PRESSURE = 1,   ///< Capteur de pression
    FLOW = 2,       ///< Capteur de flux d'air
    BOTH = 3        ///< Pression + Flux
};

/**
 * @enum ResponseCurve
 * @brief Type de courbe de réponse
 */
enum class ResponseCurve : uint8_t {
    LINEAR = 0,      ///< Linéaire
    EXPONENTIAL = 1, ///< Exponentielle
    LOGARITHMIC = 2, ///< Logarithmique
    S_CURVE = 3      ///< Courbe en S
};

/**
 * @struct AirCapabilities
 * @brief Capacités de contrôle par souffle/air (Bloc 4)
 * 
 * @details
 * Structure retournée par le Bloc 4 du protocole Custom SysEx.
 * Détaille les capacités de contrôle par souffle de l'instrument.
 * 
 * Format du message Bloc 4:
 * F0 7D <DeviceID> 04 02
 * <BreathType>         // Type de capteur (0-3)
 * <BreathCC>           // CC utilisé (0-127)
 * <MinValue>           // Valeur min (0-127)
 * <MaxValue>           // Valeur max (0-127)
 * <Sensitivity>        // Sensibilité (0-127, 64=normal)
 * <ResponseCurve>      // Courbe de réponse (0-3)
 * <Reserved[8]>        // 8 bytes réservés
 * F7
 */
struct AirCapabilities {
    BreathType breathType;        ///< Type de capteur
    uint8_t breathCC;             ///< Numéro CC utilisé
    uint8_t minValue;             ///< Valeur minimale du capteur
    uint8_t maxValue;             ///< Valeur maximale du capteur
    uint8_t sensitivity;          ///< Sensibilité (64 = normal)
    ResponseCurve responseCurve;  ///< Type de courbe de réponse
    
    /**
     * @brief Constructeur par défaut
     */
    AirCapabilities()
        : breathType(BreathType::NONE)
        , breathCC(2)  // CC2 par défaut (Breath Controller)
        , minValue(0)
        , maxValue(127)
        , sensitivity(64)
        , responseCurve(ResponseCurve::LINEAR) {}
    
    /**
     * @brief Vérifie si l'instrument a un capteur de souffle
     */
    bool hasBreathControl() const {
        return breathType != BreathType::NONE;
    }
    
    /**
     * @brief Vérifie si c'est un capteur de pression
     */
    bool hasPressureSensor() const {
        return breathType == BreathType::PRESSURE || 
               breathType == BreathType::BOTH;
    }
    
    /**
     * @brief Vérifie si c'est un capteur de flux
     */
    bool hasFlowSensor() const {
        return breathType == BreathType::FLOW || 
               breathType == BreathType::BOTH;
    }
    
    /**
     * @brief Obtient le nom du type de capteur
     */
    std::string getBreathTypeName() const {
        switch (breathType) {
            case BreathType::NONE: return "None";
            case BreathType::PRESSURE: return "Pressure";
            case BreathType::FLOW: return "Flow";
            case BreathType::BOTH: return "Pressure + Flow";
            default: return "Unknown";
        }
    }
    
    /**
     * @brief Obtient le nom de la courbe de réponse
     */
    std::string getResponseCurveName() const {
        switch (responseCurve) {
            case ResponseCurve::LINEAR: return "Linear";
            case ResponseCurve::EXPONENTIAL: return "Exponential";
            case ResponseCurve::LOGARITHMIC: return "Logarithmic";
            case ResponseCurve::S_CURVE: return "S-Curve";
            default: return "Unknown";
        }
    }
    
    /**
     * @brief Calcule le pourcentage de sensibilité
     */
    float getSensitivityPercent() const {
        // 64 = 100%, 0 = 0%, 127 = 198%
        return (sensitivity * 100.0f) / 64.0f;
    }
    
    /**
     * @brief Obtient la plage dynamique
     */
    uint8_t getDynamicRange() const {
        return maxValue - minValue;
    }
    
    /**
     * @brief Convertit en JSON
     */
    json toJson() const {
        json j;
        
        j["hasBreathControl"] = hasBreathControl();
        
        if (hasBreathControl()) {
            j["breathType"]["code"] = static_cast<uint8_t>(breathType);
            j["breathType"]["name"] = getBreathTypeName();
            j["breathType"]["hasPressure"] = hasPressureSensor();
            j["breathType"]["hasFlow"] = hasFlowSensor();
            
            j["breathCC"] = breathCC;
            
            j["range"]["min"] = minValue;
            j["range"]["max"] = maxValue;
            j["range"]["dynamic"] = getDynamicRange();
            
            j["sensitivity"]["value"] = sensitivity;
            j["sensitivity"]["percent"] = getSensitivityPercent();
            
            j["responseCurve"]["code"] = static_cast<uint8_t>(responseCurve);
            j["responseCurve"]["name"] = getResponseCurveName();
        }
        
        return j;
    }
    
    /**
     * @brief Convertit en string descriptif
     */
    std::string toString() const {
        if (!hasBreathControl()) {
            return "No breath control";
        }
        
        char buf[128];
        snprintf(buf, sizeof(buf), "%s sensor (CC%d), range %d-%d, %s curve",
                getBreathTypeName().c_str(), breathCC, minValue, maxValue,
                getResponseCurveName().c_str());
        
        return std::string(buf);
    }
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER AirCapabilities.h
// ============================================================================

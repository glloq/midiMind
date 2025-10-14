// ============================================================================
// Fichier: src/midi/sysex/LightCapabilities.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Structure pour les Capacités Lumières Custom SysEx (protocole 0x7D)
//   Bloc 5 - Détail des capacités LED/lumière
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
 * @enum LedType
 * @brief Type de LED
 */
enum class LedType : uint8_t {
    NONE = 0,       ///< Pas de LED
    SINGLE = 1,     ///< LED simple (on/off)
    RGB = 2,        ///< LED RGB
    RGBW = 3        ///< LED RGBW (RGB + White)
};

/**
 * @enum LedProtocol
 * @brief Protocole de contrôle LED
 */
enum class LedProtocol : uint8_t {
    NONE = 0,       ///< Aucun protocole
    WS2812 = 1,     ///< WS2812/WS2812B (NeoPixel)
    APA102 = 2,     ///< APA102 (DotStar)
    DMX = 3         ///< DMX512
};

/**
 * @struct AnimationSupport
 * @brief Animations supportées (bitfield)
 */
struct AnimationSupport {
    uint8_t flags;
    
    bool rainbow() const { return (flags & 0x01) != 0; }
    bool pulse() const { return (flags & 0x02) != 0; }
    bool chase() const { return (flags & 0x04) != 0; }
    bool strobe() const { return (flags & 0x08) != 0; }
    bool fade() const { return (flags & 0x10) != 0; }
    bool sparkle() const { return (flags & 0x20) != 0; }
    
    std::vector<std::string> getList() const {
        std::vector<std::string> anims;
        if (rainbow()) anims.push_back("Rainbow");
        if (pulse()) anims.push_back("Pulse");
        if (chase()) anims.push_back("Chase");
        if (strobe()) anims.push_back("Strobe");
        if (fade()) anims.push_back("Fade");
        if (sparkle()) anims.push_back("Sparkle");
        return anims;
    }
};

/**
 * @struct LightCapabilities
 * @brief Capacités LED/lumière (Bloc 5)
 * 
 * @details
 * Structure retournée par le Bloc 5 du protocole Custom SysEx.
 * Détaille les capacités LED de l'instrument.
 * 
 * Format du message Bloc 5:
 * F0 7D <DeviceID> 05 02
 * <LedCount>           // Nombre de LEDs (0-255)
 * <LedType>            // Type (0-3)
 * <Protocol>           // Protocole (0-3)
 * <Brightness>         // Luminosité par défaut (0-127)
 * <AnimationSupport>   // Animations supportées (bitfield)
 * <Reserved[12]>       // 12 bytes réservés
 * F7
 */
struct LightCapabilities {
    uint8_t ledCount;                 ///< Nombre de LEDs
    LedType ledType;                  ///< Type de LED
    LedProtocol protocol;             ///< Protocole de contrôle
    uint8_t defaultBrightness;        ///< Luminosité par défaut (0-127)
    AnimationSupport animationSupport; ///< Animations supportées
    
    /**
     * @brief Constructeur par défaut
     */
    LightCapabilities()
        : ledCount(0)
        , ledType(LedType::NONE)
        , protocol(LedProtocol::NONE)
        , defaultBrightness(64)
        , animationSupport({0}) {}
    
    /**
     * @brief Vérifie si l'instrument a des LEDs
     */
    bool hasLights() const {
        return ledCount > 0 && ledType != LedType::NONE;
    }
    
    /**
     * @brief Vérifie si les LEDs sont RGB
     */
    bool isRGB() const {
        return ledType == LedType::RGB || ledType == LedType::RGBW;
    }
    
    /**
     * @brief Vérifie si les LEDs ont une composante blanche
     */
    bool hasWhiteChannel() const {
        return ledType == LedType::RGBW;
    }
    
    /**
     * @brief Obtient le nom du type de LED
     */
    std::string getLedTypeName() const {
        switch (ledType) {
            case LedType::NONE: return "None";
            case LedType::SINGLE: return "Single Color";
            case LedType::RGB: return "RGB";
            case LedType::RGBW: return "RGBW";
            default: return "Unknown";
        }
    }
    
    /**
     * @brief Obtient le nom du protocole
     */
    std::string getProtocolName() const {
        switch (protocol) {
            case LedProtocol::NONE: return "None";
            case LedProtocol::WS2812: return "WS2812";
            case LedProtocol::APA102: return "APA102";
            case LedProtocol::DMX: return "DMX512";
            default: return "Unknown";
        }
    }
    
    /**
     * @brief Calcule le pourcentage de luminosité par défaut
     */
    float getBrightnessPercent() const {
        return (defaultBrightness * 100.0f) / 127.0f;
    }
    
    /**
     * @brief Obtient le nombre de canaux par LED
     */
    uint8_t getChannelsPerLed() const {
        switch (ledType) {
            case LedType::SINGLE: return 1;
            case LedType::RGB: return 3;
            case LedType::RGBW: return 4;
            default: return 0;
        }
    }
    
    /**
     * @brief Calcule le nombre total de canaux
     */
    uint16_t getTotalChannels() const {
        return ledCount * getChannelsPerLed();
    }
    
    /**
     * @brief Convertit en JSON
     */
    json toJson() const {
        json j;
        
        j["hasLights"] = hasLights();
        
        if (hasLights()) {
            j["ledCount"] = ledCount;
            
            j["ledType"]["code"] = static_cast<uint8_t>(ledType);
            j["ledType"]["name"] = getLedTypeName();
            j["ledType"]["isRGB"] = isRGB();
            j["ledType"]["hasWhite"] = hasWhiteChannel();
            j["ledType"]["channelsPerLed"] = getChannelsPerLed();
            j["ledType"]["totalChannels"] = getTotalChannels();
            
            j["protocol"]["code"] = static_cast<uint8_t>(protocol);
            j["protocol"]["name"] = getProtocolName();
            
            j["brightness"]["default"] = defaultBrightness;
            j["brightness"]["percent"] = getBrightnessPercent();
            
            j["animations"]["supported"] = animationSupport.getList();
            j["animations"]["flags"] = animationSupport.flags;
        }
        
        return j;
    }
    
    /**
     * @brief Convertit en string descriptif
     */
    std::string toString() const {
        if (!hasLights()) {
            return "No lights";
        }
        
        char buf[128];
        snprintf(buf, sizeof(buf), "%d x %s LEDs (%s), brightness: %d%%",
                ledCount, getLedTypeName().c_str(), getProtocolName().c_str(),
                static_cast<int>(getBrightnessPercent()));
        
        return std::string(buf);
    }
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER LightCapabilities.h
// ============================================================================

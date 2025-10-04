// ============================================================================
// Fichier: src/midi/sysex/DeviceIdentity.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Structures pour l'identification des périphériques MIDI via SysEx.
//   Permet de récupérer le fabricant, le modèle, et la version d'un device.
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

/**
 * @struct ManufacturerInfo
 * @brief Informations sur un fabricant MIDI
 */
struct ManufacturerInfo {
    std::vector<uint8_t> id;    ///< ID fabricant (1 ou 3 bytes)
    std::string name;           ///< Nom du fabricant
    std::string region;         ///< Région (American, European, Japanese, Other)
    
    /**
     * @brief Constructeur par défaut
     */
    ManufacturerInfo() = default;
    
    /**
     * @brief Constructeur avec ID simple (1 byte)
     */
    ManufacturerInfo(uint8_t singleByteId, const std::string& name, const std::string& region)
        : id{singleByteId}
        , name(name)
        , region(region) {}
    
    /**
     * @brief Constructeur avec ID étendu (3 bytes)
     */
    ManufacturerInfo(uint8_t byte1, uint8_t byte2, uint8_t byte3, 
                     const std::string& name, const std::string& region)
        : id{byte1, byte2, byte3}
        , name(name)
        , region(region) {}
    
    /**
     * @brief Vérifie si l'ID est valide
     */
    bool isValid() const {
        return !id.empty() && (id.size() == 1 || id.size() == 3);
    }
    
    /**
     * @brief Vérifie si c'est un ID étendu
     */
    bool isExtended() const {
        return id.size() == 3;
    }
    
    /**
     * @brief Convertit en string hexadécimal
     */
    std::string toString() const {
        std::string result;
        for (size_t i = 0; i < id.size(); ++i) {
            char buf[8];
            snprintf(buf, sizeof(buf), "%02X", id[i]);
            result += buf;
            if (i < id.size() - 1) {
                result += " ";
            }
        }
        return result;
    }
    
    /**
     * @brief Convertit en JSON
     */
    json toJson() const {
        json j;
        j["id"] = toString();
        j["name"] = name;
        j["region"] = region;
        j["is_extended"] = isExtended();
        return j;
    }
};

/**
 * @struct DeviceIdentity
 * @brief Identité complète d'un périphérique MIDI
 * 
 * @details
 * Structure retournée en réponse à un Identity Request.
 * Contient toutes les informations d'identification du device.
 * 
 * Format du message:
 * F0 7E <device> 06 02 <manufacturer> <family> <member> <version> F7
 */
struct DeviceIdentity {
    uint8_t deviceId;               ///< Device ID (0x00-0x7F, 0x7F = tous)
    ManufacturerInfo manufacturer;  ///< Informations fabricant
    uint16_t familyCode;            ///< Code famille du produit
    uint16_t modelNumber;           ///< Numéro de modèle
    uint32_t versionNumber;         ///< Version du firmware (4 bytes)
    
    std::string deviceName;         ///< Nom du device (déduit ou configuré)
    std::string firmwareVersion;    ///< Version formatée (ex: "1.2.3.4")
    
    /**
     * @brief Constructeur par défaut
     */
    DeviceIdentity()
        : deviceId(0)
        , familyCode(0)
        , modelNumber(0)
        , versionNumber(0) {}
    
    /**
     * @brief Vérifie si l'identité est valide
     */
    bool isValid() const {
        return manufacturer.isValid();
    }
    
    /**
     * @brief Formate la version du firmware
     */
    std::string formatFirmwareVersion() const {
        uint8_t v1 = (versionNumber >> 24) & 0xFF;
        uint8_t v2 = (versionNumber >> 16) & 0xFF;
        uint8_t v3 = (versionNumber >> 8) & 0xFF;
        uint8_t v4 = versionNumber & 0xFF;
        
        char buf[32];
        snprintf(buf, sizeof(buf), "%d.%d.%d.%d", v1, v2, v3, v4);
        
        return std::string(buf);
    }
    
    /**
     * @brief Génère un nom de device descriptif
     */
    std::string generateDeviceName() const {
        if (!deviceName.empty()) {
            return deviceName;
        }
        
        std::string name = manufacturer.name;
        
        if (!name.empty()) {
            name += " ";
        }
        
        // Ajouter le modèle si connu
        if (modelNumber != 0) {
            name += "Model " + std::to_string(modelNumber);
        } else if (familyCode != 0) {
            name += "Family " + std::to_string(familyCode);
        } else {
            name += "Device";
        }
        
        return name;
    }
    
    /**
     * @brief Convertit en JSON
     */
    json toJson() const {
        json j;
        j["device_id"] = deviceId;
        j["manufacturer"] = manufacturer.toJson();
        j["family_code"] = familyCode;
        j["model_number"] = modelNumber;
        j["version_number"] = versionNumber;
        j["firmware_version"] = formatFirmwareVersion();
        j["device_name"] = generateDeviceName();
        return j;
    }
    
    /**
     * @brief Convertit en string descriptif
     */
    std::string toString() const {
        std::ostringstream oss;
        oss << generateDeviceName();
        oss << " (ID: " << static_cast<int>(deviceId) << ")";
        oss << " - Firmware: " << formatFirmwareVersion();
        return oss.str();
    }
};

/**
 * @struct DeviceCapabilities
 * @brief Capacités d'un périphérique MIDI (optionnel)
 * 
 * @details
 * Certains devices peuvent répondre avec des informations supplémentaires
 * sur leurs capacités (nombre de canaux, polyphonie, etc.)
 */
struct DeviceCapabilities {
    uint8_t midiChannels;           ///< Nombre de canaux MIDI supportés
    uint16_t polyphony;             ///< Polyphonie maximale
    bool supportsGM;                ///< Support General MIDI
    bool supportsGM2;               ///< Support General MIDI 2
    bool supportsMTS;               ///< Support MIDI Tuning Standard
    bool supportsSysEx;             ///< Support SysEx
    
    std::vector<uint8_t> supportedControllers; ///< Controllers supportés
    std::vector<uint8_t> supportedNotes;       ///< Notes supportées (percussion)
    
    /**
     * @brief Constructeur par défaut
     */
    DeviceCapabilities()
        : midiChannels(16)
        , polyphony(0)
        , supportsGM(false)
        , supportsGM2(false)
        , supportsMTS(false)
        , supportsSysEx(false) {}
    
    /**
     * @brief Convertit en JSON
     */
    json toJson() const {
        json j;
        j["midi_channels"] = midiChannels;
        j["polyphony"] = polyphony;
        j["supports_gm"] = supportsGM;
        j["supports_gm2"] = supportsGM2;
        j["supports_mts"] = supportsMTS;
        j["supports_sysex"] = supportsSysEx;
        j["supported_controllers"] = supportedControllers;
        j["supported_notes"] = supportedNotes;
        return j;
    }
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER DeviceIdentity.h
// ============================================================================
// ============================================================================
// Fichier: src/midi/sysex/SysExParser.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Parser de messages System Exclusive (SysEx).
//   Analyse et extrait les informations des messages SysEx reçus.
//
// Responsabilités:
//   - Parser les messages Universal SysEx
//   - Extraire les Device Identity
//   - Décoder les messages spécifiques aux fabricants
//   - Valider les checksums (si présents)
//
// Thread-safety: Oui (méthodes statiques sans état)
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include <memory>
#include <optional>
#include "SysExMessage.h"
#include "DeviceIdentity.h"
#include "UniversalSysEx.h"
#include "../core/Logger.h"

namespace midiMind {

/**
 * @class SysExParser
 * @brief Parser de messages SysEx
 * 
 * @details
 * Classe utilitaire pour parser les messages SysEx.
 * Toutes les méthodes sont statiques.
 * 
 * Thread-safety: Oui
 * 
 * @example Utilisation
 * ```cpp
 * SysExMessage msg(data);
 * 
 * if (SysExParser::isIdentityReply(msg)) {
 *     auto identity = SysExParser::parseIdentityReply(msg);
 *     if (identity) {
 *         Logger::info("Device: " + identity->toString());
 *     }
 * }
 * ```
 */
class SysExParser {
public:
    // ========================================================================
    // DÉTECTION DE TYPE
    // ========================================================================
    
    /**
     * @brief Vérifie si c'est un Identity Request
     * 
     * Format: F0 7E <device> 06 01 F7
     * 
     * @param msg Message SysEx
     * @return true Si c'est un Identity Request
     */
    static bool isIdentityRequest(const SysExMessage& msg) {
        if (!msg.isValid() || msg.getSize() != 6) {
            return false;
        }
        
        return msg.getManufacturerId() == SysEx::UNIVERSAL_NON_REALTIME &&
               msg.getSubId1() == SysEx::NonRealTime::GENERAL_INFO &&
               msg.getSubId2() == SysEx::GeneralInfo::IDENTITY_REQUEST;
    }
    
    /**
     * @brief Vérifie si c'est un Identity Reply
     * 
     * Format: F0 7E <device> 06 02 <manufacturer> <family> <member> <version> F7
     * 
     * @param msg Message SysEx
     * @return true Si c'est un Identity Reply
     */
    static bool isIdentityReply(const SysExMessage& msg) {
        if (!msg.isValid() || msg.getSize() < 11) {
            return false;
        }
        
        return msg.getManufacturerId() == SysEx::UNIVERSAL_NON_REALTIME &&
               msg.getSubId1() == SysEx::NonRealTime::GENERAL_INFO &&
               msg.getSubId2() == SysEx::GeneralInfo::IDENTITY_REPLY;
    }
    
    /**
     * @brief Vérifie si c'est un message General MIDI
     */
    static bool isGeneralMidi(const SysExMessage& msg) {
        if (!msg.isValid() || msg.getSize() < 5) {
            return false;
        }
        
        return msg.getManufacturerId() == SysEx::UNIVERSAL_NON_REALTIME &&
               msg.getSubId1() == SysEx::NonRealTime::GENERAL_MIDI;
    }
    
    /**
     * @brief Vérifie si c'est un message Device Control
     */
    static bool isDeviceControl(const SysExMessage& msg) {
        if (!msg.isValid() || msg.getSize() < 5) {
            return false;
        }
        
        return msg.getManufacturerId() == SysEx::UNIVERSAL_REALTIME &&
               msg.getSubId1() == SysEx::RealTime::DEVICE_CONTROL;
    }
    
    // ========================================================================
    // PARSING - IDENTITY
    // ========================================================================
    
    /**
     * @brief Parse un Identity Reply
     * 
     * Format:
     * F0 7E <device> 06 02 <manufacturer> <family_lsb> <family_msb> 
     * <member_lsb> <member_msb> <version[4]> F7
     * 
     * @param msg Message SysEx
     * @return std::optional<DeviceIdentity> Identité ou nullopt si échec
     */
    static std::optional<DeviceIdentity> parseIdentityReply(const SysExMessage& msg) {
        if (!isIdentityReply(msg)) {
            Logger::warn("SysExParser", "Not an Identity Reply message");
            return std::nullopt;
        }
        
        const auto& data = msg.getRawData();
        
        DeviceIdentity identity;
        
        // Device ID
        identity.deviceId = data[2];
        
        // Manufacturer ID
        size_t offset = 5;
        identity.manufacturer = parseManufacturerId(data, offset);
        
        if (!identity.manufacturer.isValid()) {
            Logger::error("SysExParser", "Invalid manufacturer ID");
            return std::nullopt;
        }
        
        // Family code (2 bytes, LSB first)
        if (offset + 1 >= data.size() - 1) {
            Logger::error("SysExParser", "Incomplete family code");
            return std::nullopt;
        }
        
        identity.familyCode = data[offset] | (data[offset + 1] << 7);
        offset += 2;
        
        // Model number (2 bytes, LSB first)
        if (offset + 1 >= data.size() - 1) {
            Logger::error("SysExParser", "Incomplete model number");
            return std::nullopt;
        }
        
        identity.modelNumber = data[offset] | (data[offset + 1] << 7);
        offset += 2;
        
        // Version (4 bytes)
        if (offset + 3 >= data.size() - 1) {
            Logger::error("SysExParser", "Incomplete version number");
            return std::nullopt;
        }
        
        identity.versionNumber = (data[offset] << 24) | 
                                (data[offset + 1] << 16) |
                                (data[offset + 2] << 8) | 
                                data[offset + 3];
        
        identity.firmwareVersion = identity.formatFirmwareVersion();
        identity.deviceName = identity.generateDeviceName();
        
        Logger::info("SysExParser", "Parsed Identity: " + identity.toString());
        
        return identity;
    }
    
    // ========================================================================
    // PARSING - GENERAL MIDI
    // ========================================================================
    
    /**
     * @brief Parse un message General MIDI
     * 
     * Format:
     * F0 7E <device> 09 <sub-id2> F7
     * 
     * Sub-ID2:
     * - 01: GM System On
     * - 02: GM System Off
     * - 03: GM2 System On
     * 
     * @param msg Message SysEx
     * @return std::optional<uint8_t> Sub-ID2 ou nullopt
     */
    static std::optional<uint8_t> parseGeneralMidi(const SysExMessage& msg) {
        if (!isGeneralMidi(msg)) {
            return std::nullopt;
        }
        
        return msg.getSubId2();
    }
    
    // ========================================================================
    // PARSING - DEVICE CONTROL
    // ========================================================================
    
    /**
     * @brief Parse un message Device Control (Master Volume)
     * 
     * Format:
     * F0 7F <device> 04 01 <lsb> <msb> F7
     * 
     * @param msg Message SysEx
     * @return std::optional<uint16_t> Volume (0-16383) ou nullopt
     */
    static std::optional<uint16_t> parseMasterVolume(const SysExMessage& msg) {
        if (!isDeviceControl(msg) || msg.getSize() != 8) {
            return std::nullopt;
        }
        
        if (msg.getSubId2() != SysEx::DeviceControl::MASTER_VOLUME) {
            return std::nullopt;
        }
        
        const auto& data = msg.getRawData();
        
        uint8_t lsb = data[5];
        uint8_t msb = data[6];
        
        return lsb | (msb << 7);
    }
    
    /**
     * @brief Parse un message Device Control (Master Fine Tuning)
     * 
     * Format:
     * F0 7F <device> 04 03 <lsb> <msb> F7
     * 
     * @param msg Message SysEx
     * @return std::optional<int16_t> Tuning en cents (-8192 à +8191)
     */
    static std::optional<int16_t> parseMasterFineTuning(const SysExMessage& msg) {
        if (!isDeviceControl(msg) || msg.getSize() != 8) {
            return std::nullopt;
        }
        
        if (msg.getSubId2() != SysEx::DeviceControl::MASTER_FINE_TUNING) {
            return std::nullopt;
        }
        
        const auto& data = msg.getRawData();
        
        uint8_t lsb = data[5];
        uint8_t msb = data[6];
        
        uint16_t value = lsb | (msb << 7);
        
        // Convertir en signed (8192 = centre)
        return static_cast<int16_t>(value) - 8192;
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    /**
     * @brief Parse un Manufacturer ID
     * 
     * L'ID peut être sur 1 byte ou 3 bytes (extended).
     * 
     * @param data Données du message
     * @param offset Offset actuel (sera mis à jour)
     * @return ManufacturerInfo Info fabricant
     */
    static ManufacturerInfo parseManufacturerId(const std::vector<uint8_t>& data, 
                                                size_t& offset) {
        ManufacturerInfo info;
        
        if (offset >= data.size()) {
            return info;
        }
        
        uint8_t firstByte = data[offset];
        
        if (SysEx::isExtendedManufacturerId(firstByte)) {
            // ID étendu (3 bytes)
            if (offset + 2 >= data.size()) {
                Logger::error("SysExParser", "Incomplete extended manufacturer ID");
                return info;
            }
            
            info.id = {data[offset], data[offset + 1], data[offset + 2]};
            offset += 3;
        } else {
            // ID simple (1 byte)
            info.id = {firstByte};
            offset += 1;
        }
        
        // TODO: Lookup dans la base de données des fabricants
        // Pour l'instant, juste définir quelques noms connus
        if (info.id.size() == 1) {
            info.name = getManufacturerName(info.id[0]);
            info.region = getManufacturerRegion(info.id[0]);
        }
        
        return info;
    }
    
    /**
     * @brief Calcule et vérifie un checksum (si le format le supporte)
     * 
     * @param data Données à vérifier
     * @param expectedChecksum Checksum attendu
     * @return true Si le checksum est valide
     */
    static bool verifyChecksum(const std::vector<uint8_t>& data, uint8_t expectedChecksum) {
        uint8_t sum = 0;
        
        for (uint8_t byte : data) {
            sum += byte;
        }
        
        // Checksum 7-bit
        sum = (128 - (sum & 0x7F)) & 0x7F;
        
        return sum == expectedChecksum;
    }
    
    /**
     * @brief Décode des données 7-bit en 8-bit
     * 
     * Certains messages SysEx encodent les données 8-bit en 7-bit
     * pour éviter les bytes > 0x7F.
     * 
     * @param data Données 7-bit
     * @return std::vector<uint8_t> Données 8-bit
     */
    static std::vector<uint8_t> decode7to8bit(const std::vector<uint8_t>& data) {
        std::vector<uint8_t> result;
        
        // Format: chaque 8 bytes 7-bit → 7 bytes 8-bit
        size_t pos = 0;
        
        while (pos < data.size()) {
            if (pos + 7 >= data.size()) {
                break; // Pas assez de données
            }
            
            uint8_t msbs = data[pos++];
            
            for (int i = 0; i < 7 && pos < data.size(); ++i) {
                uint8_t byte = data[pos++];
                
                // Ajouter le MSB
                if (msbs & (1 << i)) {
                    byte |= 0x80;
                }
                
                result.push_back(byte);
            }
        }
        
        return result;
    }

private:
    /**
     * @brief Récupère le nom d'un fabricant depuis son ID
     */
    static std::string getManufacturerName(uint8_t id) {
        // Quelques fabricants courants
        switch (id) {
            case 0x01: return "Sequential Circuits";
            case 0x04: return "Moog";
            case 0x06: return "Lexicon";
            case 0x07: return "Kurzweil";
            case 0x0F: return "Ensoniq";
            case 0x10: return "Oberheim";
            case 0x11: return "Apple";
            case 0x40: return "Kawai";
            case 0x41: return "Roland";
            case 0x42: return "Korg";
            case 0x43: return "Yamaha";
            case 0x44: return "Casio";
            case 0x47: return "Akai";
            default: return "Unknown (" + std::to_string(id) + ")";
        }
    }
    
    /**
     * @brief Récupère la région d'un fabricant
     */
    static std::string getManufacturerRegion(uint8_t id) {
        if (id >= 0x00 && id <= 0x1F) return "American";
        if (id >= 0x20 && id <= 0x3F) return "European";
        if (id >= 0x40 && id <= 0x5F) return "Japanese";
        return "Other";
    }
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER SysExParser.h
// ============================================================================
// ============================================================================
// Fichier: src/midi/sysex/SysExBuilder.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Constructeur de messages System Exclusive (SysEx).
//   Facilite la création de messages SysEx standards.
//
// Responsabilités:
//   - Construire des Identity Request
//   - Construire des messages General MIDI
//   - Construire des messages Device Control
//   - Encoder les données correctement
//
// Thread-safety: Oui (méthodes statiques sans état)
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include <vector>
#include <cstdint>
#include "SysExMessage.h"
#include "UniversalSysEx.h"
#include "../../core/Logger.h"
#include "../../core/Error.h"

namespace midiMind {

/**
 * @class SysExBuilder
 * @brief Constructeur de messages SysEx
 * 
 * @details
 * Classe utilitaire pour construire des messages SysEx standards.
 * Toutes les méthodes sont statiques.
 * 
 * Thread-safety: Oui
 * 
 * @example Utilisation
 * ```cpp
 * // Créer un Identity Request
 * auto request = SysExBuilder::createIdentityRequest(0x7F);
 * 
 * // Envoyer via MIDI
 * midiOut->send(request.toBytes());
 * ```
 */
class SysExBuilder {
public:
    // ========================================================================
    // IDENTITY
    // ========================================================================
    
    /**
     * @brief Crée un Identity Request
     * 
     * Format: F0 7E <device> 06 01 F7
     * 
     * @param deviceId ID du device (0x00-0x7F, 0x7F = tous)
     * @return SysExMessage Message Identity Request
     * 
     * @example
     * ```cpp
     * // Demander l'identité de tous les devices
     * auto request = SysExBuilder::createIdentityRequest(0x7F);
     * ```
     */
    static SysExMessage createIdentityRequest(uint8_t deviceId = SysEx::DEVICE_ID_ALL) {
        std::vector<uint8_t> data = {
            SysEx::SOX,
            SysEx::UNIVERSAL_NON_REALTIME,
            deviceId,
            SysEx::NonRealTime::GENERAL_INFO,
            SysEx::GeneralInfo::IDENTITY_REQUEST,
            SysEx::EOX
        };
        
        Logger::debug("SysExBuilder", "Created Identity Request for device " + 
                     std::to_string(deviceId));
        
        return SysExMessage(data);
    }
    
    /**
     * @brief Crée un Identity Reply
     * 
     * Format: F0 7E <device> 06 02 <manufacturer> <family> <member> <version> F7
     * 
     * @param deviceId ID du device
     * @param manufacturerId ID du fabricant
     * @param familyCode Code famille
     * @param modelNumber Numéro de modèle
     * @param versionNumber Version du firmware
     * @return SysExMessage Message Identity Reply
     */
    static SysExMessage createIdentityReply(uint8_t deviceId,
                                           uint8_t manufacturerId,
                                           uint16_t familyCode,
                                           uint16_t modelNumber,
                                           uint32_t versionNumber) {
        std::vector<uint8_t> data = {
            SysEx::SOX,
            SysEx::UNIVERSAL_NON_REALTIME,
            deviceId,
            SysEx::NonRealTime::GENERAL_INFO,
            SysEx::GeneralInfo::IDENTITY_REPLY,
            manufacturerId,
            static_cast<uint8_t>(familyCode & 0x7F),        // LSB
            static_cast<uint8_t>((familyCode >> 7) & 0x7F), // MSB
            static_cast<uint8_t>(modelNumber & 0x7F),       // LSB
            static_cast<uint8_t>((modelNumber >> 7) & 0x7F),// MSB
            static_cast<uint8_t>((versionNumber >> 24) & 0x7F),
            static_cast<uint8_t>((versionNumber >> 16) & 0x7F),
            static_cast<uint8_t>((versionNumber >> 8) & 0x7F),
            static_cast<uint8_t>(versionNumber & 0x7F),
            SysEx::EOX
        };
        
        return SysExMessage(data);
    }
    
    // ========================================================================
    // GENERAL MIDI
    // ========================================================================
    
    /**
     * @brief Crée un GM System On
     * 
     * Format: F0 7E <device> 09 01 F7
     * 
     * @param deviceId ID du device
     * @return SysExMessage Message GM System On
     */
    static SysExMessage createGMSystemOn(uint8_t deviceId = SysEx::DEVICE_ID_ALL) {
        std::vector<uint8_t> data = {
            SysEx::SOX,
            SysEx::UNIVERSAL_NON_REALTIME,
            deviceId,
            SysEx::NonRealTime::GENERAL_MIDI,
            SysEx::GeneralMidi::GM_SYSTEM_ON,
            SysEx::EOX
        };
        
        Logger::debug("SysExBuilder", "Created GM System On");
        
        return SysExMessage(data);
    }
    
    /**
     * @brief Crée un GM System Off
     * 
     * Format: F0 7E <device> 09 02 F7
     * 
     * @param deviceId ID du device
     * @return SysExMessage Message GM System Off
     */
    static SysExMessage createGMSystemOff(uint8_t deviceId = SysEx::DEVICE_ID_ALL) {
        std::vector<uint8_t> data = {
            SysEx::SOX,
            SysEx::UNIVERSAL_NON_REALTIME,
            deviceId,
            SysEx::NonRealTime::GENERAL_MIDI,
            SysEx::GeneralMidi::GM_SYSTEM_OFF,
            SysEx::EOX
        };
        
        Logger::debug("SysExBuilder", "Created GM System Off");
        
        return SysExMessage(data);
    }
    
    /**
     * @brief Crée un GM2 System On
     * 
     * Format: F0 7E <device> 09 03 F7
     * 
     * @param deviceId ID du device
     * @return SysExMessage Message GM2 System On
     */
    static SysExMessage createGM2SystemOn(uint8_t deviceId = SysEx::DEVICE_ID_ALL) {
        std::vector<uint8_t> data = {
            SysEx::SOX,
            SysEx::UNIVERSAL_NON_REALTIME,
            deviceId,
            SysEx::NonRealTime::GENERAL_MIDI,
            SysEx::GeneralMidi::GM2_SYSTEM_ON,
            SysEx::EOX
        };
        
        Logger::debug("SysExBuilder", "Created GM2 System On");
        
        return SysExMessage(data);
    }
    
    // ========================================================================
    // DEVICE CONTROL
    // ========================================================================
    
    /**
     * @brief Crée un Master Volume
     * 
     * Format: F0 7F <device> 04 01 <lsb> <msb> F7
     * 
     * @param deviceId ID du device
     * @param volume Volume (0-16383, 16383 = 100%)
     * @return SysExMessage Message Master Volume
     * 
     * @example
     * ```cpp
     * // Régler le volume à 75%
     * auto msg = SysExBuilder::createMasterVolume(0x7F, 12287);
     * ```
     */
    static SysExMessage createMasterVolume(uint8_t deviceId, uint16_t volume) {
        // Limiter à 14 bits
        volume = std::min(volume, static_cast<uint16_t>(16383));
        
        std::vector<uint8_t> data = {
            SysEx::SOX,
            SysEx::UNIVERSAL_REALTIME,
            deviceId,
            SysEx::RealTime::DEVICE_CONTROL,
            SysEx::DeviceControl::MASTER_VOLUME,
            static_cast<uint8_t>(volume & 0x7F),        // LSB
            static_cast<uint8_t>((volume >> 7) & 0x7F), // MSB
            SysEx::EOX
        };
        
        Logger::debug("SysExBuilder", "Created Master Volume: " + std::to_string(volume));
        
        return SysExMessage(data);
    }
    
    /**
     * @brief Crée un Master Balance
     * 
     * Format: F0 7F <device> 04 02 <lsb> <msb> F7
     * 
     * @param deviceId ID du device
     * @param balance Balance (0-16383, 8192 = centre)
     * @return SysExMessage Message Master Balance
     */
    static SysExMessage createMasterBalance(uint8_t deviceId, uint16_t balance) {
        balance = std::min(balance, static_cast<uint16_t>(16383));
        
        std::vector<uint8_t> data = {
            SysEx::SOX,
            SysEx::UNIVERSAL_REALTIME,
            deviceId,
            SysEx::RealTime::DEVICE_CONTROL,
            SysEx::DeviceControl::MASTER_BALANCE,
            static_cast<uint8_t>(balance & 0x7F),
            static_cast<uint8_t>((balance >> 7) & 0x7F),
            SysEx::EOX
        };
        
        return SysExMessage(data);
    }
    
    /**
     * @brief Crée un Master Fine Tuning
     * 
     * Format: F0 7F <device> 04 03 <lsb> <msb> F7
     * 
     * @param deviceId ID du device
     * @param cents Tuning en cents (-8192 à +8191, 0 = centre)
     * @return SysExMessage Message Master Fine Tuning
     * 
     * @example
     * ```cpp
     * // Augmenter le pitch de 50 cents
     * auto msg = SysExBuilder::createMasterFineTuning(0x7F, 50);
     * ```
     */
    static SysExMessage createMasterFineTuning(uint8_t deviceId, int16_t cents) {
        // Limiter à -8192/+8191
        cents = std::max(static_cast<int16_t>(-8192), std::min(cents, static_cast<int16_t>(8191)));
        
        // Convertir en unsigned (8192 = centre)
        uint16_t value = static_cast<uint16_t>(cents + 8192);
        
        std::vector<uint8_t> data = {
            SysEx::SOX,
            SysEx::UNIVERSAL_REALTIME,
            deviceId,
            SysEx::RealTime::DEVICE_CONTROL,
            SysEx::DeviceControl::MASTER_FINE_TUNING,
            static_cast<uint8_t>(value & 0x7F),
            static_cast<uint8_t>((value >> 7) & 0x7F),
            SysEx::EOX
        };
        
        Logger::debug("SysExBuilder", "Created Master Fine Tuning: " + std::to_string(cents) + " cents");
        
        return SysExMessage(data);
    }
    
    /**
     * @brief Crée un Master Coarse Tuning
     * 
     * Format: F0 7F <device> 04 04 <lsb> <msb> F7
     * 
     * @param deviceId ID du device
     * @param semitones Tuning en demi-tons (-64 à +63, 0 = centre)
     * @return SysExMessage Message Master Coarse Tuning
     */
    static SysExMessage createMasterCoarseTuning(uint8_t deviceId, int8_t semitones) {
        // Limiter à -64/+63
        semitones = std::max(static_cast<int8_t>(-64), std::min(semitones, static_cast<int8_t>(63)));
        
        // Convertir en unsigned (64 = centre)
        uint16_t value = static_cast<uint16_t>(semitones + 64) << 7; // MSB uniquement
        
        std::vector<uint8_t> data = {
            SysEx::SOX,
            SysEx::UNIVERSAL_REALTIME,
            deviceId,
            SysEx::RealTime::DEVICE_CONTROL,
            SysEx::DeviceControl::MASTER_COARSE_TUNING,
            0x00, // LSB (non utilisé)
            static_cast<uint8_t>((value >> 7) & 0x7F), // MSB
            SysEx::EOX
        };
        
        Logger::debug("SysExBuilder", "Created Master Coarse Tuning: " + 
                     std::to_string(semitones) + " semitones");
        
        return SysExMessage(data);
    }
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    /**
     * @brief Encode des données 8-bit en 7-bit
     * 
     * Format: chaque 7 bytes 8-bit → 8 bytes 7-bit
     * Le premier byte contient les MSBs de chaque byte suivant.
     * 
     * @param data Données 8-bit
     * @return std::vector<uint8_t> Données 7-bit
     */
    static std::vector<uint8_t> encode8to7bit(const std::vector<uint8_t>& data) {
        std::vector<uint8_t> result;
        
        for (size_t pos = 0; pos < data.size(); pos += 7) {
            uint8_t msbs = 0;
            size_t count = std::min(size_t(7), data.size() - pos);
            
            // Extraire les MSBs
            for (size_t i = 0; i < count; ++i) {
                if (data[pos + i] & 0x80) {
                    msbs |= (1 << i);
                }
            }
            
            result.push_back(msbs);
            
            // Ajouter les 7 bits de poids faible
            for (size_t i = 0; i < count; ++i) {
                result.push_back(data[pos + i] & 0x7F);
            }
        }
        
        return result;
    }
    
    /**
     * @brief Calcule un checksum 7-bit
     * 
     * @param data Données à sommer
     * @return uint8_t Checksum (7 bits)
     */
    static uint8_t calculateChecksum(const std::vector<uint8_t>& data) {
        uint8_t sum = 0;
        
        for (uint8_t byte : data) {
            sum += byte;
        }
        
        // Checksum = (128 - (sum & 0x7F)) & 0x7F
        return (128 - (sum & 0x7F)) & 0x7F;
    }
    
    /**
     * @brief Crée un message SysEx personnalisé
     * 
     * @param manufacturerId ID du fabricant
     * @param data Données du message (sans F0 ni F7)
     * @return SysExMessage Message complet
     */
    static SysExMessage createCustom(uint8_t manufacturerId, 
                                     const std::vector<uint8_t>& data) {
        std::vector<uint8_t> message;
        message.reserve(data.size() + 3);
        
        message.push_back(SysEx::SOX);
        message.push_back(manufacturerId);
        message.insert(message.end(), data.begin(), data.end());
        message.push_back(SysEx::EOX);
        
        return SysExMessage(message);
    }
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER SysExBuilder.h
// ============================================================================
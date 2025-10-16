// ============================================================================
// Fichier: backend/src/midi/sysex/SysExParser.h
// Version: 3.0.2 - CORRECTION DES REDÉFINITIONS
// ============================================================================

// CORRECTIFS APPLIQUÉS:
// - ✅ Suppression de la redéfinition de DeviceIdentity (utilise DeviceIdentity.h)
// - ✅ Ajout des constantes manquantes dans UniversalSysEx.h (MTC, etc.)
// - ✅ Correction manufacturerId → manufacturer dans parseIdentityReply
// ============================================================================

#pragma once

#include "SysExMessage.h"
#include "DeviceIdentity.h"  // ✅ Utilise la définition officielle
#include "UniversalSysEx.h"
#include <optional>
#include <vector>
#include <cstdint>

namespace midiMind {

// ============================================================================
// STRUCTURES POUR PARSING
// ============================================================================

/**
 * @struct MTCFullFrame
 * @brief MIDI Time Code Full Frame
 */
struct MTCFullFrame {
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
    uint8_t frames;
    uint8_t frameRate;  // 0=24fps, 1=25fps, 2=29.97fps, 3=30fps
};

/**
 * @struct SampleDumpHeader
 * @brief En-tête de Sample Dump
 */
struct SampleDumpHeader {
    uint8_t deviceId;
    uint16_t sampleNumber;
    uint8_t sampleFormat;
    uint32_t samplePeriod;
    uint32_t sampleLength;
    uint32_t loopStart;
    uint32_t loopEnd;
    uint8_t loopType;
};

// ============================================================================
// CLASSE SYSEXPARSER
// ============================================================================

/**
 * @class SysExParser
 * @brief Parser pour messages System Exclusive (SysEx)
 */
class SysExParser {
public:
    // ========================================================================
    // MÉTHODES DE VÉRIFICATION DE TYPE
    // ========================================================================
    
    /**
     * @brief Vérifie si c'est un Identity Reply
     */
    static bool isIdentityReply(const SysExMessage& msg) {
        if (!msg.isUniversal() || msg.getSize() < 5) {
            return false;
        }
        return msg.getManufacturerId() == SysEx::UNIVERSAL_NON_REALTIME &&
               msg.getSubId1() == SysEx::NonRealTime::GENERAL_INFO &&
               msg.getSubId2() == SysEx::GeneralInfo::IDENTITY_REPLY;
    }
    
    /**
     * @brief Vérifie si c'est un Identity Request
     */
    static bool isIdentityRequest(const SysExMessage& msg) {
        if (!msg.isUniversal() || msg.getSize() < 5) {
            return false;
        }
        return msg.getManufacturerId() == SysEx::UNIVERSAL_NON_REALTIME &&
               msg.getSubId1() == SysEx::NonRealTime::GENERAL_INFO &&
               msg.getSubId2() == SysEx::GeneralInfo::IDENTITY_REQUEST;
    }
    
    /**
     * @brief Vérifie si c'est un message General MIDI
     */
    static bool isGeneralMidi(const SysExMessage& msg) {
        if (!msg.isUniversal() || msg.getSize() < 4) {
            return false;
        }
        return msg.getManufacturerId() == SysEx::UNIVERSAL_NON_REALTIME &&
               msg.getSubId1() == SysEx::NonRealTime::GENERAL_MIDI;
    }
    
    /**
     * @brief Vérifie si c'est un message Device Control
     */
    static bool isDeviceControl(const SysExMessage& msg) {
        if (!msg.isUniversal() || msg.getSize() < 4) {
            return false;
        }
        return msg.getManufacturerId() == SysEx::UNIVERSAL_REALTIME &&
               msg.getSubId1() == SysEx::RealTime::DEVICE_CONTROL;
    }
    
    /**
     * @brief ✅ CORRECTION: Vérifie si c'est un message MTC
     */
    static bool isMTC(const SysExMessage& msg) {
        if (!msg.isUniversal() || msg.getSize() < 4) {
            return false;
        }
        return msg.getManufacturerId() == SysEx::UNIVERSAL_REALTIME &&
               msg.getSubId1() == SysEx::RealTime::MIDI_TIME_CODE;
    }
    
    /**
     * @brief ✅ CORRECTION: Vérifie si c'est un message Sample Dump
     */
    static bool isSampleDump(const SysExMessage& msg) {
        if (!msg.isUniversal() || msg.getSize() < 4) {
            return false;
        }
        return msg.getManufacturerId() == SysEx::UNIVERSAL_NON_REALTIME &&
               msg.getSubId1() == SysEx::NonRealTime::SAMPLE_DUMP;
    }
    
    /**
     * @brief Vérifie si c'est un message File Dump
     */
    static bool isFileDump(const SysExMessage& msg) {
        if (!msg.isUniversal() || msg.getSize() < 4) {
            return false;
        }
        return msg.getManufacturerId() == SysEx::UNIVERSAL_NON_REALTIME &&
               msg.getSubId1() == SysEx::NonRealTime::FILE_DUMP;
    }
    
    /**
     * @brief ✅ CORRECTION: Vérifie si c'est un message Tuning Standard
     */
    static bool isTuningStandard(const SysExMessage& msg) {
        if (!msg.isUniversal() || msg.getSize() < 4) {
            return false;
        }
        return msg.getManufacturerId() == SysEx::UNIVERSAL_NON_REALTIME &&
               msg.getSubId1() == SysEx::NonRealTime::MIDI_TUNING_STANDARD;
    }
    
    // ========================================================================
    // PARSING - IDENTITY
    // ========================================================================
    
    /**
     * @brief ✅ CORRECTION: Parse un Identity Reply message
     * Format: F0 7E <device> 06 02 <mfg> <family:2> <model:2> <version:4> F7
     */
    static std::optional<DeviceIdentity> parseIdentityReply(const SysExMessage& msg) {
        if (!isIdentityReply(msg) || msg.getSize() < 15) {
            return std::nullopt;
        }
        
        const auto& data = msg.getRawData();
        
        DeviceIdentity identity;
        identity.deviceId = data[2];
        
        // ✅ CORRECTION: Utilise 'manufacturer' au lieu de 'manufacturerId'
        identity.manufacturer.id = data[5];
        
        // Family Code (2 bytes, LSB first)
        identity.familyCode = data[6] | (data[7] << 7);
        
        // Model Number (2 bytes, LSB first)
        identity.modelNumber = data[8] | (data[9] << 7);
        
        // Version Number (4 bytes)
        identity.softwareRevision = {
            data[10],
            data[11],
            data[12],
            data[13]
        };
        
        return identity;
    }
    
    // ========================================================================
    // PARSING - MTC
    // ========================================================================
    
    /**
     * @brief Parse un MTC Full Frame
     */
    static std::optional<MTCFullFrame> parseMTCFullFrame(const SysExMessage& msg) {
        if (!isMTC(msg) || msg.getSize() < 10) {
            return std::nullopt;
        }
        
        const auto& data = msg.getRawData();
        
        // Vérifier Sub-ID2 = 0x01 (Full Message)
        if (msg.getSubId2() != 0x01) {
            return std::nullopt;
        }
        
        MTCFullFrame frame;
        frame.hours = data[5] & 0x1F;
        frame.frameRate = (data[5] >> 5) & 0x03;
        frame.minutes = data[6];
        frame.seconds = data[7];
        frame.frames = data[8];
        
        return frame;
    }
    
    // ========================================================================
    // PARSING - DEVICE CONTROL
    // ========================================================================
    
    /**
     * @brief Parse Device Control message
     */
    static std::optional<uint8_t> parseDeviceControl(const SysExMessage& msg) {
        if (!isDeviceControl(msg)) {
            return std::nullopt;
        }
        
        return msg.getSubId2();
    }
    
    // ========================================================================
    // PARSING - SAMPLE DUMP
    // ========================================================================
    
    /**
     * @brief Parse Sample Dump Header
     */
    static std::optional<SampleDumpHeader> parseSampleDumpHeader(const SysExMessage& msg);
    
    // ========================================================================
    // ENCODAGE / DÉCODAGE
    // ========================================================================
    
    /**
     * @brief Encode des données 8-bit en 7-bit pour SysEx
     */
    static std::vector<uint8_t> encode8bitTo7bit(const std::vector<uint8_t>& data8bit);
    
    /**
     * @brief Décode des données 7-bit en 8-bit
     */
    static std::vector<uint8_t> decode7bitTo8bit(const std::vector<uint8_t>& data7bit);
    
    // ========================================================================
    // CHECKSUM
    // ========================================================================
    
    /**
     * @brief Calcule le checksum 7-bit
     */
    static uint8_t calculateChecksum(const std::vector<uint8_t>& data);
    
    /**
     * @brief Vérifie le checksum
     */
    static bool verifyChecksum(const std::vector<uint8_t>& data, uint8_t expectedChecksum);
    
    // ========================================================================
    // UTILITAIRES
    // ========================================================================
    
    /**
     * @brief Vérifie si un manufacturer ID est valide
     */
    static bool isValidManufacturerId(uint8_t id);
    
    /**
     * @brief Convertit le type de message en string
     */
    static std::string messageTypeToString(const SysExMessage& msg);
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER SysExParser.h
// ============================================================================

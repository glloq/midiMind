// ============================================================================
// Fichier: src/midi/sysex/SysExParser.h
// Version: 4.0.0 - Header complet avec toutes les déclarations
// Date: 2025-10-14
// ============================================================================

#pragma once

#include "SysExMessage.h"
#include "UniversalSysEx.h"
#include <optional>
#include <string>
#include <vector>
#include <cstdint>

namespace midiMind {

// ============================================================================
// STRUCTURES DE DONNÉES
// ============================================================================

/**
 * @brief Frame rate pour MTC (MIDI Time Code)
 */
enum class MTCFrameRate : uint8_t {
    FPS_24 = 0x00,    ///< 24 fps
    FPS_25 = 0x01,    ///< 25 fps (EBU)
    FPS_30_DROP = 0x02, ///< 30 fps drop frame (NTSC)
    FPS_30 = 0x03     ///< 30 fps non-drop
};

/**
 * @brief Informations de tuning pour une note
 */
struct TuningData {
    uint8_t note;      ///< Numéro de note MIDI (0-127)
    uint8_t semitone;  ///< Semitone (0-127)
    float cents;       ///< Cents (0.0-100.0)
};

/**
 * @brief Header de Sample Dump
 */
struct SampleDumpHeader {
    uint8_t deviceId;
    uint16_t sampleNumber;
    uint8_t sampleFormat;
    uint32_t samplePeriod;
    uint32_t sampleLength;
    uint32_t sustainLoopStart;
    uint32_t sustainLoopEnd;
    uint8_t loopType;
};

/**
 * @brief Header de File Dump
 */
struct FileDumpHeader {
    uint8_t deviceId;
    std::string fileType;
    uint32_t fileLength;
    std::string fileName;
};

/**
 * @brief Bulk Tuning Dump complet
 */
struct BulkTuningDump {
    uint8_t deviceId;
    uint8_t tuningProgramNumber;
    std::string name;
    std::vector<TuningData> tuningData;
    uint8_t checksum;
};

/**
 * @brief Message MTC Full Message
 */
struct MTCFullMessage {
    uint8_t deviceId;
    MTCFrameRate frameRate;
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
    uint8_t frames;
};

/**
 * @brief Identité d'un device MIDI
 */
struct DeviceIdentity {
    uint8_t deviceId;
    uint8_t manufacturerId;
    uint16_t familyCode;
    uint16_t modelNumber;
    uint32_t versionNumber;
    
    std::string toString() const {
        return "Device ID: " + std::to_string(deviceId) +
               ", Manufacturer: 0x" + std::to_string(manufacturerId) +
               ", Family: " + std::to_string(familyCode) +
               ", Model: " + std::to_string(modelNumber) +
               ", Version: " + std::to_string(versionNumber);
    }
};

// ============================================================================
// CLASSE SYSEXPARSER
// ============================================================================

/**
 * @brief Parser pour messages System Exclusive (SysEx)
 * 
 * Cette classe fournit des méthodes statiques pour parser et analyser
 * différents types de messages SysEx selon les spécifications MIDI.
 * 
 * Fonctionnalités:
 * - Parsing des messages Universal SysEx (Real-Time et Non-Real-Time)
 * - Extraction des données manufacturer-specific
 * - Gestion du tuning, MTC, sample dump, file dump
 * - Encodage/décodage 7-bit/8-bit
 * - Calcul et vérification de checksum
 */
class SysExParser {
public:
    // ========================================================================
    // MÉTHODES DE VÉRIFICATION DE TYPE
    // ========================================================================
    
    /**
     * @brief Vérifie si c'est un Identity Reply
     * 
     * Format: F0 7E <device> 06 02 [data] F7
     * 
     * @param msg Message SysEx
     * @return true si c'est un Identity Reply
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
     * 
     * Format: F0 7E <device> 06 01 F7
     * 
     * @param msg Message SysEx
     * @return true si c'est un Identity Request
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
     * 
     * @param msg Message SysEx
     * @return true si c'est un message GM
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
     * 
     * @param msg Message SysEx
     * @return true si c'est un Device Control
     */
    static bool isDeviceControl(const SysExMessage& msg) {
        if (!msg.isUniversal() || msg.getSize() < 4) {
            return false;
        }
        return msg.getManufacturerId() == SysEx::UNIVERSAL_REALTIME &&
               msg.getSubId1() == SysEx::RealTime::DEVICE_CONTROL;
    }
    
    /**
     * @brief Vérifie si c'est un message MTC (MIDI Time Code)
     * 
     * @param msg Message SysEx
     * @return true si c'est un message MTC
     */
    static bool isMTC(const SysExMessage& msg) {
        if (!msg.isUniversal() || msg.getSize() < 4) {
            return false;
        }
        return msg.getManufacturerId() == SysEx::UNIVERSAL_REALTIME &&
               msg.getSubId1() == SysEx::RealTime::MTC;
    }
    
    /**
     * @brief Vérifie si c'est un message Sample Dump
     * 
     * @param msg Message SysEx
     * @return true si c'est un Sample Dump
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
     * 
     * @param msg Message SysEx
     * @return true si c'est un File Dump
     */
    static bool isFileDump(const SysExMessage& msg) {
        if (!msg.isUniversal() || msg.getSize() < 4) {
            return false;
        }
        return msg.getManufacturerId() == SysEx::UNIVERSAL_NON_REALTIME &&
               msg.getSubId1() == SysEx::NonRealTime::FILE_DUMP;
    }
    
    /**
     * @brief Vérifie si c'est un message Tuning Standard
     * 
     * @param msg Message SysEx
     * @return true si c'est un Tuning Standard
     */
    static bool isTuningStandard(const SysExMessage& msg) {
        if (!msg.isUniversal() || msg.getSize() < 4) {
            return false;
        }
        return msg.getManufacturerId() == SysEx::UNIVERSAL_NON_REALTIME &&
               msg.getSubId1() == SysEx::NonRealTime::TUNING_STANDARD;
    }
    
    // ========================================================================
    // PARSING - IDENTITY
    // ========================================================================
    
    /**
     * @brief Parse un Identity Reply message
     * 
     * Format: F0 7E <device> 06 02 <mfg> <family:2> <model:2> <version:4> F7
     * 
     * @param msg Message SysEx
     * @return DeviceIdentity ou nullopt si parsing échoue
     */
    static std::optional<DeviceIdentity> parseIdentityReply(const SysExMessage& msg) {
        if (!isIdentityReply(msg) || msg.getSize() < 15) {
            return std::nullopt;
        }
        
        const auto& data = msg.getRawData();
        
        DeviceIdentity identity;
        identity.deviceId = data[2];
        identity.manufacturerId = data[5];
        
        // Family Code (2 bytes, LSB first)
        identity.familyCode = data[6] | (data[7] << 7);
        
        // Model Number (2 bytes, LSB first)
        identity.modelNumber = data[8] | (data[9] << 7);
        
        // Version Number (4 bytes)
        identity.versionNumber = (data[10] << 24) | (data[11] << 16) |
                                 (data[12] << 8) | data[13];
        
        return identity;
    }
    
    // ========================================================================
    // PARSING - DEVICE CONTROL
    // ========================================================================
    
    /**
     * @brief Parse un Device Control message
     * 
     * Format: F0 7F <device> 04 <sub-id2> [data] F7
     * 
     * @param msg Message SysEx
     * @return Sub-ID #2 ou nullopt si parsing échoue
     */
    static std::optional<uint8_t> parseDeviceControl(const SysExMessage& msg);
    
    // ========================================================================
    // PARSING - SAMPLE DUMP
    // ========================================================================
    
    /**
     * @brief Parse un Sample Dump Header
     * 
     * @param msg Message SysEx
     * @return SampleDumpHeader ou nullopt si parsing échoue
     */
    static std::optional<SampleDumpHeader> parseSampleDumpHeader(const SysExMessage& msg);
    
    // ========================================================================
    // PARSING - FILE DUMP
    // ========================================================================
    
    /**
     * @brief Parse un File Dump Header
     * 
     * @param msg Message SysEx
     * @return FileDumpHeader ou nullopt si parsing échoue
     */
    static std::optional<FileDumpHeader> parseFileDumpHeader(const SysExMessage& msg);
    
    // ========================================================================
    // PARSING - TUNING
    // ========================================================================
    
    /**
     * @brief Parse un Bulk Tuning Dump
     * 
     * @param msg Message SysEx
     * @return BulkTuningDump ou nullopt si parsing échoue
     */
    static std::optional<BulkTuningDump> parseBulkTuningDump(const SysExMessage& msg);
    
    // ========================================================================
    // PARSING - MIDI TIME CODE
    // ========================================================================
    
    /**
     * @brief Parse un MTC Full Message
     * 
     * Format: F0 7F <device> 01 01 <hr> <mn> <sc> <fr> F7
     * 
     * @param msg Message SysEx
     * @return MTCFullMessage ou nullopt si parsing échoue
     */
    static std::optional<MTCFullMessage> parseMTCFullMessage(const SysExMessage& msg);
    
    // ========================================================================
    // PARSING - NOTATION
    // ========================================================================
    
    /**
     * @brief Parse un message Notation Information
     * 
     * Format: F0 7F <device> 05 <sub-id2> [text] F7
     * 
     * @param msg Message SysEx
     * @return Texte de notation ou nullopt si parsing échoue
     */
    static std::optional<std::string> parseNotation(const SysExMessage& msg);
    
    // ========================================================================
    // PARSING - MANUFACTURER SPECIFIC
    // ========================================================================
    
    /**
     * @brief Extrait les données d'un message manufacturer-specific
     * 
     * @param msg Message SysEx
     * @return Données (sans F0, manufacturer ID, et F7) ou vecteur vide
     */
    static std::vector<uint8_t> extractManufacturerData(const SysExMessage& msg);
    
    // ========================================================================
    // HELPERS - MANUFACTURER DATABASE
    // ========================================================================
    
    /**
     * @brief Récupère le nom d'un fabricant depuis son ID
     * 
     * @param id Manufacturer ID
     * @return Nom du fabricant
     */
    static std::string getManufacturerName(uint8_t id);
    
    /**
     * @brief Récupère la région d'un fabricant depuis son ID
     * 
     * @param id Manufacturer ID
     * @return Région (American, European, Japanese, etc.)
     */
    static std::string getManufacturerRegion(uint8_t id);
    
    /**
     * @brief Convertit une valeur en string hexadécimal
     * 
     * @param value Valeur à convertir
     * @return String hexadécimal (ex: "7F")
     */
    static std::string toHexString(uint8_t value);
    
    // ========================================================================
    // DATA ENCODING/DECODING
    // ========================================================================
    
    /**
     * @brief Encode des données 8-bit en 7-bit
     * 
     * Utilisé pour transmettre des données binaires dans des messages SysEx
     * (qui n'acceptent que des bytes 7-bit, 0x00-0x7F).
     * 
     * Format: Pour chaque groupe de 7 bytes 8-bit:
     * - 1 byte MSB (bits 7 de chaque byte)
     * - 7 bytes de données (bits 0-6)
     * 
     * @param data8bit Données 8-bit à encoder
     * @return Données encodées en 7-bit
     */
    static std::vector<uint8_t> encode8BitTo7Bit(const std::vector<uint8_t>& data8bit);
    
    /**
     * @brief Décode des données 7-bit en 8-bit
     * 
     * @param data7bit Données 7-bit à décoder
     * @return Données décodées en 8-bit
     */
    static std::vector<uint8_t> decode7BitTo8Bit(const std::vector<uint8_t>& data7bit);
    
    /**
     * @brief Calcule un checksum 7-bit
     * 
     * Calcul: 128 - (sum & 0x7F)
     * 
     * @param data Données pour le calcul
     * @return Checksum 7-bit
     */
    static uint8_t calculateChecksum(const std::vector<uint8_t>& data);
    
    /**
     * @brief Vérifie un checksum
     * 
     * @param data Données
     * @param expectedChecksum Checksum attendu
     * @return true si le checksum est correct
     */
    static bool verifyChecksum(const std::vector<uint8_t>& data, uint8_t expectedChecksum);
    
    // ========================================================================
    // UTILITIES
    // ========================================================================
    
    /**
     * @brief Vérifie si un ID manufacturer est valide
     * 
     * @param id Manufacturer ID
     * @return true si valide
     */
    static bool isValidManufacturerId(uint8_t id);
    
    /**
     * @brief Convertit un message en string descriptif
     * 
     * @param msg Message SysEx
     * @return Description du type de message
     */
    static std::string messageTypeToString(const SysExMessage& msg);
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER SysExParser.h - Version 4.0.0
// ============================================================================

// ============================================================================
// Fichier: src/midi/sysex/CustomSysExParser.h
// Version: 3.0.1
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Parser pour messages SysEx custom du protocole MidiMind.
//   Implémentation complète du parsing pour tous les blocs (00-15).
//
// Fonctionnalités:
//   - Parsing Bloc 0 (Identity/Config)
//   - Parsing Bloc 1 (Valve States)
//   - Parsing Bloc 2 (Regulation)
//   - Décodage 7-bit vers 8-bit
//   - Validation complète
//   - Gestion d'erreurs
//
// Architecture:
//   - Header-only pour performances
//   - Fonctions static inline
//   - Utilisation du protocole CustomSysExProtocol.h
//
// Auteur: MidiMind Team
// Date: 2025-10-09
// ============================================================================

#pragma once

#include "CustomSysExProtocol.h"
#include "../../core/Logger.h"
#include <vector>
#include <optional>
#include <cstring>

namespace midiMind {
namespace CustomSysEx {

// ============================================================================
// CLASSE: CustomSysExParser
// Parser statique pour messages SysEx custom
// ============================================================================

class CustomSysExParser {
public:
    // ========================================================================
    // VALIDATION DU MESSAGE
    // ========================================================================
    
    /**
     * @brief Valide un message SysEx custom
     * @param data Données complètes du message (incluant F0 et F7)
     * @return true si le message est valide
     */
    static bool validate(const std::vector<uint8_t>& data) {
        // Vérification taille minimale
        if (data.size() < MIN_MESSAGE_SIZE) {
            Logger::error("CustomSysExParser", 
                "Message too short (min " + std::to_string(MIN_MESSAGE_SIZE) + " bytes)");
            return false;
        }
        
        // Vérifier SOX (F0)
        if (data[0] != SOX) {
            Logger::error("CustomSysExParser", "Missing SOX (0xF0)");
            return false;
        }
        
        // Vérifier EOX (F7)
        if (data[data.size() - 1] != EOX) {
            Logger::error("CustomSysExParser", "Missing EOX (0xF7)");
            return false;
        }
        
        // Vérifier Manufacturer ID
        if (data[1] != MANUFACTURER_ID_1 || 
            data[2] != MANUFACTURER_ID_2 || 
            data[3] != MANUFACTURER_ID_3) {
            Logger::error("CustomSysExParser", "Invalid Manufacturer ID");
            return false;
        }
        
        // Vérifier Device ID
        if (data[4] != DEVICE_ID) {
            Logger::error("CustomSysExParser", "Invalid Device ID");
            return false;
        }
        
        return true;
    }
    
    // ========================================================================
    // PARSING BLOC 0 : IDENTITY / CONFIG
    // ========================================================================
    
    /**
     * @brief Parse le Bloc 0 (Identity/Config) - IMPLÉMENTATION COMPLÈTE
     * @param data Données du message SysEx
     * @return Bloc0Identity si succès, std::nullopt si échec
     */
    static std::optional<Bloc0Identity> parseBloc0(const std::vector<uint8_t>& data) {
        if (!validate(data)) {
            return std::nullopt;
        }
        
        size_t offset = 5; // Après SOX + Manufacturer (3) + Device ID
        
        // Vérifier que c'est bien le Bloc 0
        if (offset >= data.size() - 1 || data[offset] != BLOC_IDENTITY_CONFIG) {
            Logger::error("CustomSysExParser", "Not a Bloc 0 message");
            return std::nullopt;
        }
        offset++;
        
        Bloc0Identity identity;
        
        // 1. Unique ID (4 bytes encodés 7-bit → 28-bit)
        if (offset + 4 > data.size() - 1) {
            Logger::error("CustomSysExParser", "Incomplete Unique ID");
            return std::nullopt;
        }
        
        identity.uniqueId = decode28BitFrom7Bit(
            data[offset], data[offset + 1], data[offset + 2], data[offset + 3]
        );
        offset += 4;
        
        // 2. Name (null-terminated string, max 16 chars)
        std::string name;
        while (offset < data.size() - 1 && data[offset] != 0x00) {
            name += static_cast<char>(data[offset]);
            offset++;
            
            if (name.length() > 16) {
                Logger::error("CustomSysExParser", "Name too long (max 16 chars)");
                return std::nullopt;
            }
        }
        
        if (offset >= data.size() - 1 || data[offset] != 0x00) {
            Logger::error("CustomSysExParser", "Name not null-terminated");
            return std::nullopt;
        }
        
        identity.name = name;
        offset++;
        
        // Vérifier qu'il reste assez de bytes pour les champs suivants
        if (offset + 9 > data.size() - 1) {
            Logger::error("CustomSysExParser", "Incomplete Bloc 0 data");
            return std::nullopt;
        }
        
        // 3. Firmware version (3 bytes: major, minor, patch)
        identity.firmwareVersion.major = data[offset++];
        identity.firmwareVersion.minor = data[offset++];
        identity.firmwareVersion.patch = data[offset++];
        
        // 4. Hardware version (2 bytes: major, minor)
        identity.hardwareVersion.major = data[offset++];
        identity.hardwareVersion.minor = data[offset++];
        
        // 5. Number of valves (1 byte)
        identity.numValves = data[offset++];
        
        if (identity.numValves > MAX_VALVES) {
            Logger::warn("CustomSysExParser", 
                "Number of valves exceeds maximum (" + 
                std::to_string(identity.numValves) + " > " + 
                std::to_string(MAX_VALVES) + ")");
        }
        
        // 6. MIDI channels (2 bytes: input, output)
        identity.midiChannelIn = data[offset++] & 0x0F;   // 0-15
        identity.midiChannelOut = data[offset++] & 0x0F;  // 0-15
        
        // 7. Features bitmap (1 byte)
        identity.features = data[offset++];
        
        // Décoder les features
        identity.hasRegulation = (identity.features & FEATURE_REGULATION) != 0;
        identity.hasCCMapping = (identity.features & FEATURE_CC_MAPPING) != 0;
        identity.hasAirControl = (identity.features & FEATURE_AIR_CONTROL) != 0;
        identity.hasTuning = (identity.features & FEATURE_TUNING) != 0;
        
        Logger::info("CustomSysExParser", 
            "Bloc 0 parsed: ID=" + std::to_string(identity.uniqueId) + 
            ", Name=" + identity.name + 
            ", FW=" + identity.firmwareVersion.toString() +
            ", Valves=" + std::to_string(identity.numValves));
        
        return identity;
    }
    
    // ========================================================================
    // PARSING BLOC 1 : VALVE STATES
    // ========================================================================
    
    /**
     * @brief Parse le Bloc 1 (Valve States) - IMPLÉMENTATION COMPLÈTE
     * @param data Données du message SysEx
     * @return Vecteur de ValveState si succès
     */
    static std::vector<Bloc1ValveState> parseBloc1(const std::vector<uint8_t>& data) {
        if (!validate(data)) {
            return {};
        }
        
        size_t offset = 5; // Après SOX + Manufacturer (3) + Device ID
        
        // Vérifier que c'est bien le Bloc 1
        if (offset >= data.size() - 1 || data[offset] != BLOC_VALVE_STATES) {
            Logger::error("CustomSysExParser", "Not a Bloc 1 message");
            return {};
        }
        offset++;
        
        std::vector<Bloc1ValveState> valves;
        
        // Lire le nombre de valves
        if (offset >= data.size() - 1) {
            Logger::error("CustomSysExParser", "Missing valve count");
            return {};
        }
        
        uint8_t numValves = data[offset++];
        
        if (numValves > MAX_VALVES) {
            Logger::warn("CustomSysExParser", 
                "Number of valves exceeds maximum, clamping to " + 
                std::to_string(MAX_VALVES));
            numValves = MAX_VALVES;
        }
        
        // Parser chaque valve (6 bytes par valve)
        for (uint8_t i = 0; i < numValves; ++i) {
            if (offset + 6 > data.size() - 1) {
                Logger::error("CustomSysExParser", 
                    "Incomplete valve data for valve " + std::to_string(i));
                break;
            }
            
            Bloc1ValveState valve;
            
            // 1. Valve ID (1 byte)
            valve.valveId = data[offset++];
            
            // 2. Current position (2 bytes, 14-bit encodé en 7-bit)
            valve.currentPosition = decode14BitFrom7Bit(data[offset], data[offset + 1]);
            offset += 2;
            
            // 3. Target position (2 bytes, 14-bit encodé en 7-bit)
            valve.targetPosition = decode14BitFrom7Bit(data[offset], data[offset + 1]);
            offset += 2;
            
            // 4. Status flags (1 byte)
            valve.status = data[offset++];
            
            // Décoder les flags
            valve.isMoving = (valve.status & STATUS_MOVING) != 0;
            valve.isCalibrated = (valve.status & STATUS_CALIBRATED) != 0;
            valve.hasError = (valve.status & STATUS_ERROR) != 0;
            valve.isEnabled = (valve.status & STATUS_ENABLED) != 0;
            
            valves.push_back(valve);
        }
        
        Logger::info("CustomSysExParser", 
            "Bloc 1 parsed: " + std::to_string(valves.size()) + " valves");
        
        return valves;
    }
    
    // ========================================================================
    // PARSING BLOC 2 : REGULATION
    // ========================================================================
    
    /**
     * @brief Parse le Bloc 2 (Regulation) - IMPLÉMENTATION COMPLÈTE
     * @param data Données du message SysEx
     * @return Bloc2Regulation si succès
     */
    static std::optional<Bloc2Regulation> parseBloc2(const std::vector<uint8_t>& data) {
        if (!validate(data)) {
            return std::nullopt;
        }
        
        size_t offset = 5; // Après SOX + Manufacturer (3) + Device ID
        
        // Vérifier que c'est bien le Bloc 2
        if (offset >= data.size() - 1 || data[offset] != BLOC_REGULATION) {
            Logger::error("CustomSysExParser", "Not a Bloc 2 message");
            return std::nullopt;
        }
        offset++;
        
        // Vérifier taille minimale (9 bytes pour Bloc 2)
        if (offset + 9 > data.size() - 1) {
            Logger::error("CustomSysExParser", "Incomplete Bloc 2 data");
            return std::nullopt;
        }
        
        Bloc2Regulation regulation;
        
        // 1. PID gains (3 x 2 bytes = 6 bytes, 14-bit encodé en 7-bit)
        regulation.gainP = decode14BitFrom7Bit(data[offset], data[offset + 1]);
        offset += 2;
        
        regulation.gainI = decode14BitFrom7Bit(data[offset], data[offset + 1]);
        offset += 2;
        
        regulation.gainD = decode14BitFrom7Bit(data[offset], data[offset + 1]);
        offset += 2;
        
        // 2. Response speed (1 byte, 0-127)
        regulation.responseSpeed = data[offset++] & 0x7F;
        
        // 3. Deadzone (1 byte, 0-127)
        regulation.deadzone = data[offset++] & 0x7F;
        
        // 4. Flags (1 byte)
        regulation.flags = data[offset++];
        
        // Décoder les flags
        regulation.enabled = (regulation.flags & 0x01) != 0;
        regulation.autoCalibrate = (regulation.flags & 0x02) != 0;
        
        Logger::info("CustomSysExParser", 
            "Bloc 2 parsed: P=" + std::to_string(regulation.gainP) + 
            ", I=" + std::to_string(regulation.gainI) + 
            ", D=" + std::to_string(regulation.gainD) +
            ", Speed=" + std::to_string(regulation.responseSpeed));
        
        return regulation;
    }
    
    // ========================================================================
    // UTILITAIRES DE DÉCODAGE
    // ========================================================================
    
    /**
     * @brief Décode un nombre 14-bit depuis 2 bytes 7-bit
     * Format: MSB (bits 13-7) | LSB (bits 6-0)
     */
    static uint16_t decode14BitFrom7Bit(uint8_t msb, uint8_t lsb) {
        return ((static_cast<uint16_t>(msb & 0x7F) << 7) | 
                 static_cast<uint16_t>(lsb & 0x7F));
    }
    
    /**
     * @brief Encode un nombre 14-bit vers 2 bytes 7-bit
     */
    static void encode14BitTo7Bit(uint16_t value, uint8_t& msb, uint8_t& lsb) {
        msb = (value >> 7) & 0x7F;
        lsb = value & 0x7F;
    }
    
    /**
     * @brief Décode un nombre 28-bit depuis 4 bytes 7-bit
     * Format: B3 (bits 27-21) | B2 (bits 20-14) | B1 (bits 13-7) | B0 (bits 6-0)
     */
    static uint32_t decode28BitFrom7Bit(uint8_t b3, uint8_t b2, uint8_t b1, uint8_t b0) {
        return ((static_cast<uint32_t>(b3 & 0x7F) << 21) |
                (static_cast<uint32_t>(b2 & 0x7F) << 14) |
                (static_cast<uint32_t>(b1 & 0x7F) << 7) |
                 static_cast<uint32_t>(b0 & 0x7F));
    }
    
    /**
     * @brief Encode un nombre 28-bit vers 4 bytes 7-bit
     */
    static void encode28BitTo7Bit(uint32_t value, 
                                   uint8_t& b3, uint8_t& b2, 
                                   uint8_t& b1, uint8_t& b0) {
        b3 = (value >> 21) & 0x7F;
        b2 = (value >> 14) & 0x7F;
        b1 = (value >> 7) & 0x7F;
        b0 = value & 0x7F;
    }
    
    /**
     * @brief Décode des données 7-bit en 8-bit
     * 
     * Format : Pour chaque groupe de 8 bytes, le premier byte contient
     * les bits MSB (bit 7) des 7 bytes suivants.
     * 
     * Exemple:
     * Input:  [MSB_BYTE, b1, b2, b3, b4, b5, b6, b7, ...]
     * Output: [B1, B2, B3, B4, B5, B6, B7, ...]
     * où Bi = bi | ((MSB_BYTE & (1 << (i-1))) << (8-i))
     */
    static std::vector<uint8_t> decode7BitTo8Bit(const std::vector<uint8_t>& data7bit) {
        std::vector<uint8_t> data8bit;
        data8bit.reserve((data7bit.size() * 7) / 8);
        
        for (size_t i = 0; i < data7bit.size(); i += 8) {
            // Vérifier qu'il reste assez de bytes
            if (i + 1 > data7bit.size()) {
                break;
            }
            
            uint8_t msbByte = data7bit[i];
            size_t count = std::min(size_t(7), data7bit.size() - i - 1);
            
            for (size_t j = 0; j < count; ++j) {
                uint8_t dataByte = data7bit[i + 1 + j];
                uint8_t msb = (msbByte & (1 << j)) ? 0x80 : 0x00;
                data8bit.push_back(dataByte | msb);
            }
        }
        
        return data8bit;
    }
    
    /**
     * @brief Encode des données 8-bit en 7-bit
     */
    static std::vector<uint8_t> encode8BitTo7Bit(const std::vector<uint8_t>& data8bit) {
        std::vector<uint8_t> data7bit;
        data7bit.reserve(((data8bit.size() * 8) / 7) + 1);
        
        for (size_t i = 0; i < data8bit.size(); i += 7) {
            uint8_t msbByte = 0;
            size_t count = std::min(size_t(7), data8bit.size() - i);
            
            // Extraire les MSB
            for (size_t j = 0; j < count; ++j) {
                if (data8bit[i + j] & 0x80) {
                    msbByte |= (1 << j);
                }
            }
            
            data7bit.push_back(msbByte);
            
            // Ajouter les 7 bits de données
            for (size_t j = 0; j < count; ++j) {
                data7bit.push_back(data8bit[i + j] & 0x7F);
            }
        }
        
        return data7bit;
    }
    
    // ========================================================================
    // UTILITAIRES DE CRÉATION DE MESSAGES
    // ========================================================================
    
    /**
     * @brief Crée un message SysEx complet pour Bloc 0
     */
    static std::vector<uint8_t> createBloc0Message(const Bloc0Identity& identity) {
        std::vector<uint8_t> message;
        
        // Header
        message.push_back(SOX);
        message.push_back(MANUFACTURER_ID_1);
        message.push_back(MANUFACTURER_ID_2);
        message.push_back(MANUFACTURER_ID_3);
        message.push_back(DEVICE_ID);
        message.push_back(BLOC_IDENTITY_CONFIG);
        
        // Unique ID (28-bit → 4 x 7-bit)
        uint8_t b3, b2, b1, b0;
        encode28BitTo7Bit(identity.uniqueId, b3, b2, b1, b0);
        message.push_back(b3);
        message.push_back(b2);
        message.push_back(b1);
        message.push_back(b0);
        
        // Name (null-terminated)
        for (char c : identity.name) {
            message.push_back(static_cast<uint8_t>(c) & 0x7F);
        }
        message.push_back(0x00); // Null terminator
        
        // Version info
        message.push_back(identity.firmwareVersion.major);
        message.push_back(identity.firmwareVersion.minor);
        message.push_back(identity.firmwareVersion.patch);
        message.push_back(identity.hardwareVersion.major);
        message.push_back(identity.hardwareVersion.minor);
        
        // Valves and channels
        message.push_back(identity.numValves);
        message.push_back(identity.midiChannelIn);
        message.push_back(identity.midiChannelOut);
        
        // Features
        message.push_back(identity.features);
        
        // EOX
        message.push_back(EOX);
        
        return message;
    }
};

} // namespace CustomSysEx
} // namespace midiMind

// ============================================================================
// FIN DU FICHIER CustomSysExParser.h
// ============================================================================

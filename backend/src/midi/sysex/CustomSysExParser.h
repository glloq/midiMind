// ============================================================================
// Fichier: backend/src/midi/sysex/CustomSysExParser.h
// Version: 3.0.2 - CORRECTION DES INCLUDES
// ============================================================================

// CORRECTIFS APPLIQUÉS:
// - Ajout de l'include CustomSysExTypes.h
// - Utilisation des structures et constantes définies
// ============================================================================

#pragma once

#include "CustomSysExTypes.h"  // ✅ AJOUT: Structures et constantes
#include "SysExMessage.h"
#include "../../core/Logger.h"
#include <optional>
#include <vector>
#include <cstdint>

namespace midiMind {
namespace CustomSysEx {

/**
 * @class CustomSysExParser
 * @brief Parser pour messages SysEx personnalisés
 */
class CustomSysExParser {
public:
    // ========================================================================
    // VALIDATION
    // ========================================================================
    
    /**
     * @brief Valide un message SysEx personnalisé
     */
    static bool validate(const std::vector<uint8_t>& data) {
        // Taille minimum
        if (data.size() < MIN_MESSAGE_SIZE) {
            Logger::error("CustomSysExParser", "Message too short");
            return false;
        }
        
        // Vérifier SOX
        if (data[0] != SOX) {
            Logger::error("CustomSysExParser", "Invalid SOX");
            return false;
        }
        
        // Vérifier EOX
        if (data[data.size() - 1] != EOX) {
            Logger::error("CustomSysExParser", "Invalid EOX");
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
     * @brief Parse le Bloc 0 (Identity/Config)
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
                Logger::error("CustomSysExParser", "Name too long");
                return std::nullopt;
            }
        }
        
        if (offset >= data.size() - 1 || data[offset] != 0x00) {
            Logger::error("CustomSysExParser", "Name not null-terminated");
            return std::nullopt;
        }
        
        identity.name = name;
        offset++;
        
        // Vérifier qu'il reste assez de bytes
        if (offset + 9 > data.size() - 1) {
            Logger::error("CustomSysExParser", "Incomplete Bloc 0 data");
            return std::nullopt;
        }
        
        // 3. Firmware version
        identity.firmwareVersion.major = data[offset++];
        identity.firmwareVersion.minor = data[offset++];
        identity.firmwareVersion.patch = data[offset++];
        
        // 4. Hardware version
        identity.hardwareVersion.major = data[offset++];
        identity.hardwareVersion.minor = data[offset++];
        
        // 5. Number of valves
        identity.numValves = data[offset++];
        
        if (identity.numValves > MAX_VALVES) {
            Logger::warn("CustomSysExParser", 
                "Number of valves exceeds maximum");
        }
        
        // 6. MIDI channels
        identity.midiChannelIn = data[offset++] & 0x0F;
        identity.midiChannelOut = data[offset++] & 0x0F;
        
        // 7. Features bitmap
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
     * @brief Parse le Bloc 1 (Valve States)
     */
    static std::vector<Bloc1ValveState> parseBloc1(const std::vector<uint8_t>& data) {
        if (!validate(data)) {
            return {};
        }
        
        size_t offset = 5;
        
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
            Logger::warn("CustomSysExParser", "Clamping valve count to max");
            numValves = MAX_VALVES;
        }
        
        // Parser chaque valve (6 bytes par valve)
        for (uint8_t i = 0; i < numValves; ++i) {
            if (offset + 6 > data.size() - 1) {
                Logger::error("CustomSysExParser", "Incomplete valve data");
                break;
            }
            
            Bloc1ValveState valve;
            
            valve.valveId = data[offset++];
            valve.currentPosition = decode14BitFrom7Bit(data[offset], data[offset + 1]);
            offset += 2;
            valve.targetPosition = decode14BitFrom7Bit(data[offset], data[offset + 1]);
            offset += 2;
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
     * @brief Parse le Bloc 2 (Regulation)
     */
    static std::optional<Bloc2Regulation> parseBloc2(const std::vector<uint8_t>& data) {
        if (!validate(data)) {
            return std::nullopt;
        }
        
        size_t offset = 5;
        
        if (offset >= data.size() - 1 || data[offset] != BLOC_REGULATION) {
            Logger::error("CustomSysExParser", "Not a Bloc 2 message");
            return std::nullopt;
        }
        offset++;
        
        if (offset + 9 > data.size() - 1) {
            Logger::error("CustomSysExParser", "Incomplete Bloc 2 data");
            return std::nullopt;
        }
        
        Bloc2Regulation regulation;
        
        regulation.gainP = decode14BitFrom7Bit(data[offset], data[offset + 1]);
        offset += 2;
        
        regulation.gainI = decode14BitFrom7Bit(data[offset], data[offset + 1]);
        offset += 2;
        
        regulation.gainD = decode14BitFrom7Bit(data[offset], data[offset + 1]);
        offset += 2;
        
        regulation.responseSpeed = data[offset++] & 0x7F;
        regulation.deadzone = data[offset++] & 0x7F;
        regulation.smoothing = data[offset++] & 0x7F;
        
        Logger::info("CustomSysExParser", "Bloc 2 parsed successfully");
        
        return regulation;
    }
    
    // ========================================================================
    // CRÉATION DE MESSAGES
    // ========================================================================
    
    /**
     * @brief Crée un message SysEx complet pour Bloc 0
     */
    static std::vector<uint8_t> createBloc0Message(const Bloc0Identity& identity) {
        std::vector<uint8_t> message;
        
        message.push_back(SOX);
        message.push_back(MANUFACTURER_ID_1);
        message.push_back(MANUFACTURER_ID_2);
        message.push_back(MANUFACTURER_ID_3);
        message.push_back(DEVICE_ID);
        message.push_back(BLOC_IDENTITY_CONFIG);
        
        // Unique ID
        uint8_t b3, b2, b1, b0;
        encode28BitTo7Bit(identity.uniqueId, b3, b2, b1, b0);
        message.push_back(b3);
        message.push_back(b2);
        message.push_back(b1);
        message.push_back(b0);
        
        // Name
        for (char c : identity.name) {
            message.push_back(static_cast<uint8_t>(c) & 0x7F);
        }
        message.push_back(0x00);
        
        // Versions
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
        
        message.push_back(EOX);
        
        return message;
    }
};

} // namespace CustomSysEx
} // namespace midiMind

// ============================================================================
// FIN DU FICHIER CustomSysExParser.h
// ============================================================================

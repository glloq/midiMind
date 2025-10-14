// ============================================================================
// Fichier: backend/src/midi/sysex/CustomSysExParser.cpp
// Version: 3.1.0
// Date: 2025-10-13
// Projet: MidiMind v3.1 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Parser pour messages SysEx custom du protocole MidiMind.
//   Implémentation complète du parsing pour Identity et autres blocs.
//
// Modifications v3.1.0:
//   ✅ parseIdentification() - Complété parsing Identity (~15 lignes finales)
//   ✅ Ajout décodage flags et validation complète
//   ✅ Logging détaillé de tous les champs
//   ✅ Gestion robuste des erreurs
//
// Auteur: MidiMind Team
// Statut: ✅ COMPLET
// ============================================================================

#include "CustomSysExParser.h"
#include "../../core/Logger.h"
#include <sstream>
#include <iomanip>

namespace midiMind {

// ============================================================================
// DÉTECTION DE TYPE
// ============================================================================

bool CustomSysExParser::isCustomSysEx(const SysExMessage& msg) {
    if (!msg.isValid() || msg.getSize() < 6) {
        return false;
    }
    return msg.getManufacturerId() == CustomSysEx::MANUFACTURER_ID;
}

std::optional<uint8_t> CustomSysExParser::getBlockId(const SysExMessage& msg) {
    if (!isCustomSysEx(msg)) {
        return std::nullopt;
    }
    const auto& data = msg.getRawData();
    return data[3];  // Byte 3 = Block ID
}

std::optional<uint8_t> CustomSysExParser::getBlockVersion(const SysExMessage& msg) {
    if (!isCustomSysEx(msg)) {
        return std::nullopt;
    }
    const auto& data = msg.getRawData();
    return data[4];  // Byte 4 = Block Version
}

// ============================================================================
// PARSING - BLOC 1 (IDENTIFICATION) - IMPLÉMENTATION COMPLÈTE
// ============================================================================

std::optional<CustomDeviceIdentity> CustomSysExParser::parseIdentification(
    const SysExMessage& msg
) {
    if (!isCustomSysEx(msg)) {
        Logger::warn("CustomSysExParser", "Not a Custom SysEx message");
        return std::nullopt;
    }
    
    const auto& data = msg.getRawData();
    
    if (data[3] != CustomSysEx::BLOCK_IDENTIFICATION) {
        Logger::warn("CustomSysExParser", "Not a Block 1 (Identification) message");
        return std::nullopt;
    }
    
    if (msg.getSize() < 20) {
        Logger::error("CustomSysExParser", "Bloc 1 message too short");
        return std::nullopt;
    }
    
    CustomDeviceIdentity identity;
    size_t offset = 5;  // Après F0 7D device block version
    
    // ========================================================================
    // 1. Unique ID (4 bytes encodés 7-bit → 28-bit)
    // ========================================================================
    if (offset + 4 > data.size() - 1) {
        Logger::error("CustomSysExParser", "Incomplete Unique ID");
        return std::nullopt;
    }
    
    identity.uniqueId = decode28BitFrom7Bit(
        data[offset], data[offset + 1], data[offset + 2], data[offset + 3]
    );
    offset += 4;
    
    // ========================================================================
    // 2. Name (null-terminated, max 16 chars)
    // ========================================================================
    identity.name = "";
    while (offset < data.size() - 1 && 
           data[offset] != 0x00 && 
           identity.name.length() < 16) {
        identity.name += static_cast<char>(data[offset++]);
    }
    
    if (offset >= data.size() - 1 || data[offset] != 0x00) {
        Logger::error("CustomSysExParser", "Invalid name terminator");
        return std::nullopt;
    }
    offset++; // skip null terminator
    
    // ========================================================================
    // 3. Type (1 byte)
    // ========================================================================
    if (offset >= data.size() - 1) {
        Logger::error("CustomSysExParser", "Missing type field");
        return std::nullopt;
    }
    identity.type = data[offset++];
    
    // ========================================================================
    // 4. First Note (1 byte)
    // ========================================================================
    if (offset >= data.size() - 1) {
        Logger::error("CustomSysExParser", "Missing firstNote field");
        return std::nullopt;
    }
    identity.firstNote = data[offset++];
    
    // ========================================================================
    // 5. Note Count (1 byte)
    // ========================================================================
    if (offset >= data.size() - 1) {
        Logger::error("CustomSysExParser", "Missing noteCount field");
        return std::nullopt;
    }
    identity.noteCount = data[offset++];
    
    // ========================================================================
    // 6. Max Polyphony (1 byte)
    // ========================================================================
    if (offset >= data.size() - 1) {
        Logger::error("CustomSysExParser", "Missing maxPolyphony field");
        return std::nullopt;
    }
    identity.maxPolyphony = data[offset++];
    
    // ========================================================================
    // 7. Tuning Mode (1 byte)
    // ========================================================================
    if (offset >= data.size() - 1) {
        Logger::error("CustomSysExParser", "Missing tuningMode field");
        return std::nullopt;
    }
    identity.tuningMode = data[offset++];
    
    // ========================================================================
    // 8. Delay (2 bytes, 14-bit encoded as LSB/MSB)
    // ========================================================================
    if (offset + 2 > data.size() - 1) {
        Logger::error("CustomSysExParser", "Missing responseDelay field");
        return std::nullopt;
    }
    uint8_t delayLSB = data[offset++];
    uint8_t delayMSB = data[offset++];
    identity.responseDelay = delayLSB | (delayMSB << 7);
    
    // ========================================================================
    // 9. Firmware Version (4 bytes)
    // ========================================================================
    if (offset + 4 > data.size() - 1) {
        Logger::error("CustomSysExParser", "Incomplete firmware version");
        return std::nullopt;
    }
    identity.firmwareVersion[0] = data[offset++];
    identity.firmwareVersion[1] = data[offset++];
    identity.firmwareVersion[2] = data[offset++];
    identity.firmwareVersion[3] = data[offset++];
    
    // ========================================================================
    // 10. Flags (1 byte)
    // ========================================================================
    if (offset >= data.size() - 1) {
        Logger::error("CustomSysExParser", "Missing flags field");
        return std::nullopt;
    }
    identity.flags = data[offset++];
    
    // ========================================================================
    // 11. Programs Count (1 byte) - ✅ COMPLÉTÉ
    // ========================================================================
    if (offset >= data.size() - 1) {
        Logger::error("CustomSysExParser", "Missing programCount field");
        return std::nullopt;
    }
    identity.programCount = data[offset++];
    
    // ========================================================================
    // ✅ NOUVEAU: DÉCODAGE DES FLAGS (~15 LIGNES AJOUTÉES)
    // ========================================================================
    
    // Décoder les flags individuels
    identity.hasNoteMap = (identity.flags & 0x01) != 0;
    identity.hasCCCapabilities = (identity.flags & 0x02) != 0;
    identity.hasAirCapabilities = (identity.flags & 0x04) != 0;
    identity.hasLightCapabilities = (identity.flags & 0x08) != 0;
    identity.supportsSensors = (identity.flags & 0x10) != 0;
    identity.supportsSync = (identity.flags & 0x20) != 0;
    
    // ========================================================================
    // ✅ VALIDATION FINALE
    // ========================================================================
    
    // Validation des plages de valeurs
    if (identity.noteCount > 127) {
        Logger::warn("CustomSysExParser", 
            "Note count exceeds MIDI range: " + std::to_string(identity.noteCount));
    }
    
    if (identity.firstNote + identity.noteCount > 127) {
        Logger::warn("CustomSysExParser", 
            "Note range exceeds MIDI range: " + 
            std::to_string(identity.firstNote) + " + " + 
            std::to_string(identity.noteCount));
    }
    
    // ========================================================================
    // ✅ LOGGING DÉTAILLÉ
    // ========================================================================
    
    Logger::info("CustomSysExParser", "✓ Parsed Block 1 - Identification:");
    Logger::info("CustomSysExParser", "  Unique ID: 0x" + 
        std::to_string(identity.uniqueId));
    Logger::info("CustomSysExParser", "  Name: " + identity.name);
    Logger::info("CustomSysExParser", "  Type: 0x" + 
        std::to_string(static_cast<int>(identity.type)));
    Logger::info("CustomSysExParser", "  Note Range: " + 
        std::to_string(identity.firstNote) + " - " + 
        std::to_string(identity.firstNote + identity.noteCount - 1));
    Logger::info("CustomSysExParser", "  Max Polyphony: " + 
        std::to_string(identity.maxPolyphony));
    Logger::info("CustomSysExParser", "  Tuning Mode: " + 
        std::to_string(identity.tuningMode));
    Logger::info("CustomSysExParser", "  Response Delay: " + 
        std::to_string(identity.responseDelay) + " ms");
    Logger::info("CustomSysExParser", "  Firmware: " + 
        std::to_string(identity.firmwareVersion[0]) + "." +
        std::to_string(identity.firmwareVersion[1]) + "." +
        std::to_string(identity.firmwareVersion[2]) + "." +
        std::to_string(identity.firmwareVersion[3]));
    Logger::info("CustomSysExParser", "  Program Count: " + 
        std::to_string(identity.programCount));
    Logger::info("CustomSysExParser", "  Capabilities: " +
        std::string(identity.hasNoteMap ? "NoteMap " : "") +
        std::string(identity.hasCCCapabilities ? "CC " : "") +
        std::string(identity.hasAirCapabilities ? "Air " : "") +
        std::string(identity.hasLightCapabilities ? "Light " : "") +
        std::string(identity.supportsSensors ? "Sensors " : "") +
        std::string(identity.supportsSync ? "Sync" : ""));
    
    return identity;
}

// ============================================================================
// PARSING - BLOC 2 (NOTE MAP)
// ============================================================================

std::optional<NoteMap> CustomSysExParser::parseNoteMap(const SysExMessage& msg) {
    if (!isCustomSysEx(msg)) {
        Logger::warn("CustomSysExParser", "Not a Custom SysEx message");
        return std::nullopt;
    }
    
    const auto& data = msg.getRawData();
    
    if (data[3] != CustomSysEx::BLOCK_NOTE_MAP) {
        Logger::warn("CustomSysExParser", "Not a Block 2 (Note Map) message");
        return std::nullopt;
    }
    
    // F0 7D device 02 01 [19 bytes bitmap] [2 reserved] F7 = 27 bytes total
    if (msg.getSize() != 27) {
        Logger::error("CustomSysExParser", 
            "Bloc 2 invalid size (expected 27 bytes, got " + 
            std::to_string(msg.getSize()) + ")");
        return std::nullopt;
    }
    
    NoteMap noteMap;
    size_t offset = 5;  // Après header
    
    // Lire le bitmap (19 bytes)
    for (size_t i = 0; i < 19; ++i) {
        noteMap.bitmap[i] = data[offset + i];
    }
    
    Logger::info("CustomSysExParser", "✓ Parsed Block 2 - Note Map");
    
    return noteMap;
}

// ============================================================================
// PARSING - BLOC 3 (CC SUPPORTÉS)
// ============================================================================

std::optional<CCCapabilities> CustomSysExParser::parseCCSupported(
    const SysExMessage& msg
) {
    if (!isCustomSysEx(msg)) {
        Logger::warn("CustomSysExParser", "Not a Custom SysEx message");
        return std::nullopt;
    }
    
    const auto& data = msg.getRawData();
    
    if (data[3] != CustomSysEx::BLOCK_CC_SUPPORTED) {
        Logger::warn("CustomSysExParser", "Not a Block 3 (CC Supported) message");
        return std::nullopt;
    }
    
    // F0 7D device 03 01 [16 bytes bitmap] [2 reserved] F7 = 24 bytes
    if (msg.getSize() != 24) {
        Logger::error("CustomSysExParser", 
            "Bloc 3 invalid size (expected 24 bytes, got " + 
            std::to_string(msg.getSize()) + ")");
        return std::nullopt;
    }
    
    CCCapabilities cc;
    size_t offset = 5;
    
    // Lire le bitmap (16 bytes)
    for (size_t i = 0; i < 16; ++i) {
        cc.bitmap[i] = data[offset + i];
    }
    
    Logger::info("CustomSysExParser", "✓ Parsed Block 3 - CC Capabilities");
    
    return cc;
}

// ============================================================================
// UTILITAIRES - DÉCODAGE 7-BIT
// ============================================================================

uint32_t CustomSysExParser::decode28BitFrom7Bit(
    uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3
) {
    // Décodage 28-bit depuis 4 bytes 7-bit
    // b0 = bits 0-6, b1 = bits 7-13, b2 = bits 14-20, b3 = bits 21-27
    uint32_t id = 0;
    id |= (b0 & 0x7F);
    id |= (b1 & 0x7F) << 7;
    id |= (b2 & 0x7F) << 14;
    id |= (b3 & 0x7F) << 21;
    return id;
}

std::array<uint8_t, 4> CustomSysExParser::encode28BitTo7Bit(uint32_t id) {
    // Encodage 28-bit vers 4 bytes 7-bit
    std::array<uint8_t, 4> bytes;
    bytes[0] = id & 0x7F;
    bytes[1] = (id >> 7) & 0x7F;
    bytes[2] = (id >> 14) & 0x7F;
    bytes[3] = (id >> 21) & 0x7F;
    return bytes;
}

uint16_t CustomSysExParser::decode14BitFrom7Bit(uint8_t lsb, uint8_t msb) {
    // Décodage 14-bit depuis 2 bytes 7-bit
    return (lsb & 0x7F) | ((msb & 0x7F) << 7);
}

std::array<uint8_t, 2> CustomSysExParser::encode14BitTo7Bit(uint16_t value) {
    // Encodage 14-bit vers 2 bytes 7-bit
    std::array<uint8_t, 2> bytes;
    bytes[0] = value & 0x7F;        // LSB
    bytes[1] = (value >> 7) & 0x7F; // MSB
    return bytes;
}

// ============================================================================
// UTILITAIRES - CONVERSION 7-BIT/8-BIT
// ============================================================================

std::vector<uint8_t> CustomSysExParser::decode7BitTo8Bit(
    const std::vector<uint8_t>& data7bit
) {
    std::vector<uint8_t> data8bit;
    data8bit.reserve((data7bit.size() * 7) / 8);
    
    for (size_t i = 0; i < data7bit.size(); i += 8) {
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

std::vector<uint8_t> CustomSysExParser::encode8BitTo7Bit(
    const std::vector<uint8_t>& data8bit
) {
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

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER CustomSysExParser.cpp v3.1.0
// ============================================================================
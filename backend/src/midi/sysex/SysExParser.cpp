// ============================================================================
// Fichier: src/midi/sysex/SysExParser.cpp
// Version: 3.0.1 - Implémentation complète
// Date: 2025-10-09
// ============================================================================

#include "SysExParser.h"
#include "../../core/Logger.h"
#include <algorithm>

namespace midiMind {

// ============================================================================
// PARSING - DEVICE CONTROL
// ============================================================================

std::optional<uint8_t> SysExParser::parseDeviceControl(const SysExMessage& msg) {
    if (!isDeviceControl(msg)) {
        return std::nullopt;
    }
    
    // Format: F0 7F <device> 04 <sub-id2> [data] F7
    return msg.getSubId2();
}

// ============================================================================
// PARSING - SAMPLE DUMP
// ============================================================================

std::optional<SampleDumpHeader> SysExParser::parseSampleDumpHeader(const SysExMessage& msg) {
    if (!msg.isValid() || msg.getSize() < 21) {
        return std::nullopt;
    }
    
    const auto& data = msg.getRawData();
    
    // Vérifier que c'est bien un Sample Dump Header
    if (data[1] != SysEx::UNIVERSAL_NON_REALTIME || 
        data[3] != SysEx::NonRealTime::SAMPLE_DUMP ||
        data[4] != 0x01) { // 0x01 = Header
        return std::nullopt;
    }
    
    SampleDumpHeader header;
    header.deviceId = data[2];
    
    // Sample Number (2 bytes, 7-bit each, LSB first)
    header.sampleNumber = data[5] | (data[6] << 7);
    
    // Sample Format (1 byte)
    header.sampleFormat = data[7];
    
    // Sample Period (3 bytes, 7-bit each, LSB first)
    header.samplePeriod = data[8] | (data[9] << 7) | (data[10] << 14);
    
    // Sample Length (3 bytes, 7-bit each, LSB first)
    header.sampleLength = data[11] | (data[12] << 7) | (data[13] << 14);
    
    // Sustain Loop Start (3 bytes)
    header.sustainLoopStart = data[14] | (data[15] << 7) | (data[16] << 14);
    
    // Sustain Loop End (3 bytes)
    header.sustainLoopEnd = data[17] | (data[18] << 7) | (data[19] << 14);
    
    // Loop Type (1 byte)
    header.loopType = data[20];
    
    Logger::info("SysExParser", "Parsed Sample Dump Header: " + 
                std::to_string(header.sampleNumber) + ", " +
                std::to_string(header.sampleLength) + " bytes");
    
    return header;
}

// ============================================================================
// PARSING - FILE DUMP
// ============================================================================

std::optional<FileDumpHeader> SysExParser::parseFileDumpHeader(const SysExMessage& msg) {
    if (!msg.isValid() || msg.getSize() < 10) {
        return std::nullopt;
    }
    
    const auto& data = msg.getRawData();
    
    // Vérifier que c'est bien un File Dump Header
    if (data[1] != SysEx::UNIVERSAL_NON_REALTIME || 
        data[3] != SysEx::NonRealTime::FILE_DUMP ||
        data[4] != 0x01) { // 0x01 = Header
        return std::nullopt;
    }
    
    FileDumpHeader header;
    header.deviceId = data[2];
    
    // Type (4 bytes ASCII)
    header.fileType = std::string(data.begin() + 5, data.begin() + 9);
    
    // Length (4 bytes, 7-bit each, LSB first)
    if (data.size() >= 14) {
        header.fileLength = data[9] | (data[10] << 7) | 
                           (data[11] << 14) | (data[12] << 21);
    }
    
    // Name (variable length, null-terminated)
    size_t nameStart = 13;
    size_t nameEnd = nameStart;
    while (nameEnd < data.size() - 1 && data[nameEnd] != 0) {
        nameEnd++;
    }
    
    if (nameEnd > nameStart) {
        header.fileName = std::string(data.begin() + nameStart, 
                                     data.begin() + nameEnd);
    }
    
    Logger::info("SysExParser", "Parsed File Dump Header: " + 
                header.fileType + ", " + header.fileName);
    
    return header;
}

// ============================================================================
// PARSING - TUNING
// ============================================================================

std::optional<BulkTuningDump> SysExParser::parseBulkTuningDump(const SysExMessage& msg) {
    if (!msg.isValid() || msg.getSize() < 406) {
        Logger::warn("SysExParser", "Invalid Bulk Tuning Dump size");
        return std::nullopt;
    }
    
    const auto& data = msg.getRawData();
    
    // Vérifier format
    if (data[1] != SysEx::UNIVERSAL_NON_REALTIME || 
        data[3] != SysEx::NonRealTime::TUNING_STANDARD ||
        data[4] != 0x01) { // 0x01 = Bulk Dump Request
        return std::nullopt;
    }
    
    BulkTuningDump tuning;
    tuning.deviceId = data[2];
    tuning.tuningProgramNumber = data[5];
    
    // Name (16 bytes ASCII)
    tuning.name = std::string(data.begin() + 6, data.begin() + 22);
    
    // Tuning data (128 notes × 3 bytes = 384 bytes)
    tuning.tuningData.reserve(128);
    
    for (int note = 0; note < 128; ++note) {
        size_t offset = 22 + (note * 3);
        
        if (offset + 2 >= data.size()) {
            Logger::error("SysExParser", "Incomplete tuning data");
            return std::nullopt;
        }
        
        // 3 bytes per note: [semitone] [fraction_msb] [fraction_lsb]
        // Fraction = (fraction_msb << 7) | fraction_lsb (0-16383 for 0-100 cents)
        TuningData noteData;
        noteData.note = note;
        noteData.semitone = data[offset];
        uint16_t fraction = (data[offset + 1] << 7) | data[offset + 2];
        noteData.cents = (fraction * 100.0f) / 16383.0f;
        
        tuning.tuningData.push_back(noteData);
    }
    
    // Checksum (1 byte)
    if (data.size() >= 407) {
        tuning.checksum = data[406];
        
        // Vérifier checksum
        uint8_t calculatedSum = 0;
        for (size_t i = 1; i < 406; ++i) {
            calculatedSum ^= data[i];
        }
        
        if ((calculatedSum & 0x7F) != tuning.checksum) {
            Logger::warn("SysExParser", "Tuning dump checksum mismatch");
        }
    }
    
    Logger::info("SysExParser", "Parsed Bulk Tuning Dump: " + tuning.name);
    
    return tuning;
}

// ============================================================================
// PARSING - MIDI TIME CODE
// ============================================================================

std::optional<MTCFullMessage> SysExParser::parseMTCFullMessage(const SysExMessage& msg) {
    if (!msg.isValid() || msg.getSize() != 10) {
        return std::nullopt;
    }
    
    const auto& data = msg.getRawData();
    
    // Vérifier format: F0 7F <device> 01 01 <hr> <mn> <sc> <fr> F7
    if (data[1] != SysEx::UNIVERSAL_REALTIME || 
        data[3] != SysEx::RealTime::MTC ||
        data[4] != 0x01) { // Full Message
        return std::nullopt;
    }
    
    MTCFullMessage mtc;
    mtc.deviceId = data[2];
    
    // Rate and Hours (1 byte: [rate:2][hours:5])
    uint8_t rateAndHours = data[5];
    mtc.frameRate = static_cast<MTCFrameRate>((rateAndHours >> 5) & 0x03);
    mtc.hours = rateAndHours & 0x1F;
    
    // Minutes, Seconds, Frames
    mtc.minutes = data[6];
    mtc.seconds = data[7];
    mtc.frames = data[8];
    
    Logger::debug("SysExParser", "Parsed MTC: " + 
                 std::to_string(mtc.hours) + ":" +
                 std::to_string(mtc.minutes) + ":" +
                 std::to_string(mtc.seconds) + "." +
                 std::to_string(mtc.frames));
    
    return mtc;
}

// ============================================================================
// PARSING - NOTATION
// ============================================================================

std::optional<std::string> SysExParser::parseNotation(const SysExMessage& msg) {
    if (!msg.isValid() || msg.getSize() < 6) {
        return std::nullopt;
    }
    
    const auto& data = msg.getRawData();
    
    // Format: F0 7F <device> 05 <sub-id2> [text] F7
    if (data[1] != SysEx::UNIVERSAL_REALTIME || 
        data[3] != SysEx::RealTime::NOTATION_INFO) {
        return std::nullopt;
    }
    
    // Extraire le texte (bytes 5 à size-1, avant F7)
    if (data.size() > 6) {
        std::string text(data.begin() + 5, data.end() - 1);
        Logger::debug("SysExParser", "Parsed notation: " + text);
        return text;
    }
    
    return std::nullopt;
}

// ============================================================================
// PARSING - MANUFACTURER SPECIFIC
// ============================================================================

std::vector<uint8_t> SysExParser::extractManufacturerData(const SysExMessage& msg) {
    if (!msg.isManufacturerSpecific() || msg.getSize() < 3) {
        return {};
    }
    
    const auto& data = msg.getRawData();
    
    // Déterminer où commence la data
    size_t dataStart = 1; // Après F0
    
    // Skip manufacturer ID
    if (SysEx::isExtendedManufacturerId(data[1])) {
        dataStart = 4; // F0 + 3 bytes ID
    } else {
        dataStart = 2; // F0 + 1 byte ID
    }
    
    // Extraire jusqu'à F7
    if (dataStart >= data.size() - 1) {
        return {};
    }
    
    return std::vector<uint8_t>(data.begin() + dataStart, data.end() - 1);
}

// ============================================================================
// HELPERS - MANUFACTURER DATABASE
// ============================================================================

std::string SysExParser::getManufacturerName(uint8_t id) {
    // Quelques fabricants courants (liste étendue)
    switch (id) {
        // American Group (0x01-0x1F)
        case 0x01: return "Sequential Circuits";
        case 0x02: return "Big Briar (Moog)";
        case 0x03: return "Octave / Plateau";
        case 0x04: return "Moog";
        case 0x05: return "Passport Designs";
        case 0x06: return "Lexicon";
        case 0x07: return "Kurzweil";
        case 0x08: return "Fender";
        case 0x09: return "Gulbransen";
        case 0x0A: return "AKG Acoustics";
        case 0x0B: return "Voyce Music";
        case 0x0C: return "Waveframe";
        case 0x0D: return "ADA Signal Processors";
        case 0x0E: return "Garfield Electronics";
        case 0x0F: return "Ensoniq";
        case 0x10: return "Oberheim";
        case 0x11: return "Apple Computer";
        case 0x12: return "Grey Matter Response";
        case 0x13: return "Digidesign";
        case 0x14: return "Palm Tree Instruments";
        case 0x15: return "JLCooper Electronics";
        
        // European Group (0x20-0x3F)
        case 0x20: return "Passac";
        case 0x21: return "SIEL";
        case 0x22: return "Synthaxe";
        case 0x23: return "Stepp";
        case 0x24: return "Hohner";
        case 0x25: return "Twister";
        case 0x26: return "Solton";
        case 0x27: return "Jellinghaus MS";
        case 0x28: return "Southworth Music Systems";
        case 0x29: return "PPG";
        case 0x2A: return "JEN";
        case 0x2B: return "SSL";
        case 0x2C: return "Audio Veritrieb";
        case 0x2D: return "Neve";
        case 0x2E: return "Soundtracs Ltd.";
        case 0x2F: return "Elka";
        case 0x30: return "Dynacord";
        case 0x31: return "Jomox";
        case 0x33: return "Clavia Digital Instruments";
        
        // Japanese Group (0x40-0x5F)
        case 0x40: return "Kawai";
        case 0x41: return "Roland";
        case 0x42: return "Korg";
        case 0x43: return "Yamaha";
        case 0x44: return "Casio";
        case 0x45: return "Akai";
        case 0x46: return "Victor Company of Japan";
        case 0x47: return "Mesosha";
        case 0x48: return "Hoshino Gakki";
        case 0x49: return "Fujitsu Elect";
        case 0x4A: return "Sony";
        case 0x4B: return "Nisshin Onpa";
        case 0x4C: return "TEAC Corporation";
        case 0x4E: return "Matsushita Electric";
        case 0x4F: return "Fostex";
        case 0x50: return "Zoom";
        case 0x51: return "Matsushita Communication Industrial";
        case 0x52: return "Suzuki Musical Inst. Mfg.";
        case 0x53: return "Fuji Sound Corporation";
        case 0x54: return "Acoustic Technical Laboratory";
        
        // Educational/Special
        case 0x7D: return "Educational Use";
        case 0x7E: return "Universal Non-Realtime";
        case 0x7F: return "Universal Realtime";
        
        default: 
            return "Unknown (0x" + toHexString(id) + ")";
    }
}

std::string SysExParser::getManufacturerRegion(uint8_t id) {
    if (id >= 0x00 && id <= 0x1F) return "American";
    if (id >= 0x20 && id <= 0x3F) return "European";
    if (id >= 0x40 && id <= 0x5F) return "Japanese";
    if (id == 0x7D) return "Educational";
    if (id == 0x7E || id == 0x7F) return "Universal";
    return "Other/Special";
}

std::string SysExParser::toHexString(uint8_t value) {
    char buf[3];
    snprintf(buf, sizeof(buf), "%02X", value);
    return std::string(buf);
}

// ============================================================================
// DATA ENCODING/DECODING
// ============================================================================

std::vector<uint8_t> SysExParser::encode8BitTo7Bit(const std::vector<uint8_t>& data8bit) {
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

std::vector<uint8_t> SysExParser::decode7BitTo8Bit(const std::vector<uint8_t>& data7bit) {
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

uint8_t SysExParser::calculateChecksum(const std::vector<uint8_t>& data) {
    uint8_t sum = 0;
    
    for (uint8_t byte : data) {
        sum += byte;
    }
    
    // Checksum 7-bit (two's complement)
    return (128 - (sum & 0x7F)) & 0x7F;
}

bool SysExParser::verifyChecksum(const std::vector<uint8_t>& data, uint8_t expectedChecksum) {
    uint8_t calculated = calculateChecksum(data);
    return calculated == expectedChecksum;
}

// ============================================================================
// UTILITIES
// ============================================================================

bool SysExParser::isValidManufacturerId(uint8_t id) {
    // Les IDs 0x00, 0x7E, 0x7F ont des significations spéciales
    if (id == 0x00) return true;  // Extended ID follows
    if (id == 0x7E) return true;  // Universal Non-Realtime
    if (id == 0x7F) return true;  // Universal Realtime
    
    // Les autres IDs 0x01-0x7D sont valides
    return (id >= 0x01 && id <= 0x7D);
}

std::string SysExParser::messageTypeToString(const SysExMessage& msg) {
    if (!msg.isValid()) {
        return "Invalid";
    }
    
    if (msg.isUniversal()) {
        uint8_t subId1 = msg.getSubId1();
        uint8_t subId2 = msg.getSubId2();
        
        if (msg.getManufacturerId() == SysEx::UNIVERSAL_NON_REALTIME) {
            switch (subId1) {
                case SysEx::NonRealTime::SAMPLE_DUMP:
                    return "Sample Dump";
                case SysEx::NonRealTime::GENERAL_INFO:
                    if (subId2 == SysEx::GeneralInfo::IDENTITY_REQUEST)
                        return "Identity Request";
                    if (subId2 == SysEx::GeneralInfo::IDENTITY_REPLY)
                        return "Identity Reply";
                    return "General Information";
                case SysEx::NonRealTime::FILE_DUMP:
                    return "File Dump";
                case SysEx::NonRealTime::TUNING_STANDARD:
                    return "Tuning Standard";
                case SysEx::NonRealTime::GENERAL_MIDI:
                    return "General MIDI";
                default:
                    return "Universal Non-Realtime";
            }
        } else {
            switch (subId1) {
                case SysEx::RealTime::MTC:
                    return "MIDI Time Code";
                case SysEx::RealTime::SHOW_CONTROL:
                    return "Show Control";
                case SysEx::RealTime::NOTATION_INFO:
                    return "Notation Information";
                case SysEx::RealTime::DEVICE_CONTROL:
                    return "Device Control";
                case SysEx::RealTime::MTC_CUEING:
                    return "MTC Cueing";
                case SysEx::RealTime::MMC_COMMANDS:
                    return "MIDI Machine Control";
                case SysEx::RealTime::MMC_RESPONSES:
                    return "MMC Response";
                default:
                    return "Universal Realtime";
            }
        }
    } else if (msg.isManufacturerSpecific()) {
        uint8_t mfgId = msg.getManufacturerId();
        return "Manufacturer Specific (" + getManufacturerName(mfgId) + ")";
    }
    
    return "Unknown";
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER SysExParser.cpp - Version 3.0.1 complète
// ============================================================================

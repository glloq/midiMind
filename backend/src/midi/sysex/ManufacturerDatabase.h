// ============================================================================
// Fichier: src/midi/sysex/manufacturers/ManufacturerDatabase.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Base de données des fabricants MIDI et de leurs IDs.
//   Permet de résoudre les IDs en noms de fabricants.
//
// Référence: MIDI Manufacturers System Exclusive ID Numbers
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include <string>
#include <map>
#include <vector>
#include <optional>
#include "../DeviceIdentity.h"

namespace midiMind {

/**
 * @class ManufacturerDatabase
 * @brief Base de données des fabricants MIDI
 * 
 * @details
 * Contient les IDs de tous les fabricants MIDI connus.
 * Permet de convertir un ID en nom de fabricant.
 * 
 * Thread-safety: Oui (lecture seule après initialisation)
 * 
 * @example Utilisation
 * ```cpp
 * auto info = ManufacturerDatabase::lookup(0x41);
 * if (info) {
 *     Logger::info("Manufacturer: " + info->name); // "Roland"
 * }
 * ```
 */
class ManufacturerDatabase {
public:
    // ========================================================================
    // LOOKUP
    // ========================================================================
    
    /**
     * @brief Recherche un fabricant par son ID (1 byte)
     * 
     * @param id ID du fabricant (0x00-0x7F)
     * @return std::optional<ManufacturerInfo> Info ou nullopt
     */
    static std::optional<ManufacturerInfo> lookup(uint8_t id) {
        auto it = singleByteIds_.find(id);
        if (it != singleByteIds_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    /**
     * @brief Recherche un fabricant par son ID étendu (3 bytes)
     * 
     * @param byte1 Premier byte (toujours 0x00)
     * @param byte2 Deuxième byte
     * @param byte3 Troisième byte
     * @return std::optional<ManufacturerInfo> Info ou nullopt
     */
    static std::optional<ManufacturerInfo> lookup(uint8_t byte1, uint8_t byte2, uint8_t byte3) {
        uint32_t key = (byte1 << 16) | (byte2 << 8) | byte3;
        
        auto it = extendedIds_.find(key);
        if (it != extendedIds_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    /**
     * @brief Recherche un fabricant par son nom
     * 
     * @param name Nom du fabricant (insensible à la casse)
     * @return std::optional<ManufacturerInfo> Info ou nullopt
     */
    static std::optional<ManufacturerInfo> lookupByName(const std::string& name) {
        std::string lowerName = toLower(name);
        
        // Chercher dans les IDs simples
        for (const auto& [id, info] : singleByteIds_) {
            if (toLower(info.name) == lowerName) {
                return info;
            }
        }
        
        // Chercher dans les IDs étendus
        for (const auto& [id, info] : extendedIds_) {
            if (toLower(info.name) == lowerName) {
                return info;
            }
        }
        
        return std::nullopt;
    }
    
    // ========================================================================
    // LISTAGE
    // ========================================================================
    
    /**
     * @brief Liste tous les fabricants
     * 
     * @return std::vector<ManufacturerInfo> Liste de tous les fabricants
     */
    static std::vector<ManufacturerInfo> listAll() {
        std::vector<ManufacturerInfo> result;
        
        // Ajouter les IDs simples
        for (const auto& [id, info] : singleByteIds_) {
            result.push_back(info);
        }
        
        // Ajouter les IDs étendus
        for (const auto& [id, info] : extendedIds_) {
            result.push_back(info);
        }
        
        return result;
    }
    
    /**
     * @brief Liste les fabricants par région
     * 
     * @param region Région ("American", "European", "Japanese", "Other")
     * @return std::vector<ManufacturerInfo> Fabricants de cette région
     */
    static std::vector<ManufacturerInfo> listByRegion(const std::string& region) {
        std::vector<ManufacturerInfo> result;
        
        for (const auto& [id, info] : singleByteIds_) {
            if (info.region == region) {
                result.push_back(info);
            }
        }
        
        for (const auto& [id, info] : extendedIds_) {
            if (info.region == region) {
                result.push_back(info);
            }
        }
        
        return result;
    }

private:
    /**
     * @brief Convertit en minuscules
     */
    static std::string toLower(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }
    
    // ========================================================================
    // BASE DE DONNÉES - IDs 1 BYTE (0x01-0x7D)
    // ========================================================================
    
    static const std::map<uint8_t, ManufacturerInfo> singleByteIds_;
    
    // ========================================================================
    // BASE DE DONNÉES - IDs ÉTENDUS (0x00 + 2 bytes)
    // ========================================================================
    
    static const std::map<uint32_t, ManufacturerInfo> extendedIds_;
};

// ============================================================================
// INITIALISATION DE LA BASE DE DONNÉES
// ============================================================================

// IDs 1 byte (Groupe Américain: 0x01-0x1F)
const std::map<uint8_t, ManufacturerInfo> ManufacturerDatabase::singleByteIds_ = {
    // American Group (0x01-0x1F)
    {0x01, ManufacturerInfo(0x01, "Sequential Circuits", "American")},
    {0x02, ManufacturerInfo(0x02, "Big Briar (Moog)", "American")},
    {0x03, ManufacturerInfo(0x03, "Octave / Plateau", "American")},
    {0x04, ManufacturerInfo(0x04, "Moog", "American")},
    {0x05, ManufacturerInfo(0x05, "Passport Designs", "American")},
    {0x06, ManufacturerInfo(0x06, "Lexicon", "American")},
    {0x07, ManufacturerInfo(0x07, "Kurzweil", "American")},
    {0x08, ManufacturerInfo(0x08, "Fender", "American")},
    {0x09, ManufacturerInfo(0x09, "Gulbransen", "American")},
    {0x0A, ManufacturerInfo(0x0A, "AKG Acoustics", "American")},
    {0x0B, ManufacturerInfo(0x0B, "Voyce Music", "American")},
    {0x0C, ManufacturerInfo(0x0C, "Waveframe", "American")},
    {0x0D, ManufacturerInfo(0x0D, "ADA Signal Processors", "American")},
    {0x0E, ManufacturerInfo(0x0E, "Garfield Electronics", "American")},
    {0x0F, ManufacturerInfo(0x0F, "Ensoniq", "American")},
    {0x10, ManufacturerInfo(0x10, "Oberheim", "American")},
    {0x11, ManufacturerInfo(0x11, "Apple Computer", "American")},
    {0x12, ManufacturerInfo(0x12, "Grey Matter Response", "American")},
    {0x13, ManufacturerInfo(0x13, "Digidesign", "American")},
    {0x14, ManufacturerInfo(0x14, "Palm Tree Instruments", "American")},
    {0x15, ManufacturerInfo(0x15, "JLCooper Electronics", "American")},
    {0x16, ManufacturerInfo(0x16, "Lowrey", "American")},
    {0x17, ManufacturerInfo(0x17, "Adams-Smith", "American")},
    {0x18, ManufacturerInfo(0x18, "E-mu Systems", "American")},
    {0x19, ManufacturerInfo(0x19, "Harmony Systems", "American")},
    {0x1A, ManufacturerInfo(0x1A, "ART", "American")},
    {0x1B, ManufacturerInfo(0x1B, "Baldwin", "American")},
    {0x1C, ManufacturerInfo(0x1C, "Eventide", "American")},
    {0x1D, ManufacturerInfo(0x1D, "Inventronics", "American")},
    {0x1E, ManufacturerInfo(0x1E, "Key Concepts", "American")},
    {0x1F, ManufacturerInfo(0x1F, "Clarity", "American")},
    
    // European Group (0x20-0x3F)
    {0x20, ManufacturerInfo(0x20, "Passac", "European")},
    {0x21, ManufacturerInfo(0x21, "SIEL", "European")},
    {0x22, ManufacturerInfo(0x22, "Synthaxe", "European")},
    {0x23, ManufacturerInfo(0x23, "Stepp", "European")},
    {0x24, ManufacturerInfo(0x24, "Hohner", "European")},
    {0x25, ManufacturerInfo(0x25, "Twister", "European")},
    {0x26, ManufacturerInfo(0x26, "Solton", "European")},
    {0x27, ManufacturerInfo(0x27, "Jellinghaus MS", "European")},
    {0x28, ManufacturerInfo(0x28, "Southworth Music Systems", "European")},
    {0x29, ManufacturerInfo(0x29, "PPG", "European")},
    {0x2A, ManufacturerInfo(0x2A, "JEN", "European")},
    {0x2B, ManufacturerInfo(0x2B, "SSL", "European")},
    {0x2C, ManufacturerInfo(0x2C, "Audio Veritrieb", "European")},
    {0x2D, ManufacturerInfo(0x2D, "Neve", "European")},
    {0x2E, ManufacturerInfo(0x2E, "Soundtracs Ltd.", "European")},
    {0x2F, ManufacturerInfo(0x2F, "Elka", "European")},
    {0x30, ManufacturerInfo(0x30, "Dynacord", "European")},
    {0x31, ManufacturerInfo(0x31, "Viscount", "European")},
    {0x32, ManufacturerInfo(0x32, "Drawmer", "European")},
    {0x33, ManufacturerInfo(0x33, "Clavia Digital Instruments", "European")},
    {0x34, ManufacturerInfo(0x34, "Audio Architecture", "European")},
    {0x35, ManufacturerInfo(0x35, "General Music Corp", "European")},
    {0x36, ManufacturerInfo(0x36, "Cheetah Marketing", "European")},
    {0x37, ManufacturerInfo(0x37, "C.T.M.", "European")},
    {0x38, ManufacturerInfo(0x38, "Simmons UK", "European")},
    {0x39, ManufacturerInfo(0x39, "Soundcraft Electronics", "European")},
    {0x3A, ManufacturerInfo(0x3A, "Steinberg", "European")},
    {0x3B, ManufacturerInfo(0x3B, "Wersi", "European")},
    {0x3C, ManufacturerInfo(0x3C, "AVAB Niethammer AB", "European")},
    {0x3D, ManufacturerInfo(0x3D, "Digigram", "European")},
    {0x3E, ManufacturerInfo(0x3E, "Waldorf Electronics", "European")},
    {0x3F, ManufacturerInfo(0x3F, "Quasimidi", "European")},
    
    // Japanese Group (0x40-0x5F)
    {0x40, ManufacturerInfo(0x40, "Kawai", "Japanese")},
    {0x41, ManufacturerInfo(0x41, "Roland", "Japanese")},
    {0x42, ManufacturerInfo(0x42, "Korg", "Japanese")},
    {0x43, ManufacturerInfo(0x43, "Yamaha", "Japanese")},
    {0x44, ManufacturerInfo(0x44, "Casio", "Japanese")},
    {0x45, ManufacturerInfo(0x45, "Moridaira", "Japanese")},
    {0x46, ManufacturerInfo(0x46, "Kamiya Studio", "Japanese")},
    {0x47, ManufacturerInfo(0x47, "Akai", "Japanese")},
    {0x48, ManufacturerInfo(0x48, "Victor", "Japanese")},
    {0x49, ManufacturerInfo(0x49, "Mesosha", "Japanese")},
    {0x4A, ManufacturerInfo(0x4A, "Hoshino Gakki", "Japanese")},
    {0x4B, ManufacturerInfo(0x4B, "Fujitsu", "Japanese")},
    {0x4C, ManufacturerInfo(0x4C, "Sony", "Japanese")},
    {0x4D, ManufacturerInfo(0x4D, "Nisshin Onpa", "Japanese")},
    {0x4E, ManufacturerInfo(0x4E, "TEAC", "Japanese")},
    {0x4F, ManufacturerInfo(0x4F, "Matsushita Electric", "Japanese")},
    {0x50, ManufacturerInfo(0x50, "Fostex", "Japanese")},
    {0x51, ManufacturerInfo(0x51, "Zoom", "Japanese")},
    {0x52, ManufacturerInfo(0x52, "Midori Electronics", "Japanese")},
    {0x53, ManufacturerInfo(0x53, "Matsushita Communication", "Japanese")},
    {0x54, ManufacturerInfo(0x54, "Suzuki", "Japanese")},
    {0x55, ManufacturerInfo(0x55, "Fuji Sound", "Japanese")},
    {0x56, ManufacturerInfo(0x56, "Acoustic Technical Laboratory", "Japanese")},
    {0x57, ManufacturerInfo(0x57, "Faith", "Japanese")},
    {0x58, ManufacturerInfo(0x58, "Internet Corporation", "Japanese")},
    {0x59, ManufacturerInfo(0x59, "Seekers Co.", "Japanese")},
    {0x5A, ManufacturerInfo(0x5A, "SD Card Association", "Japanese")},
    {0x5B, ManufacturerInfo(0x5B, "Crimson Technology", "Japanese")},
    {0x5C, ManufacturerInfo(0x5C, "Softbank Mobile", "Japanese")},
    {0x5D, ManufacturerInfo(0x5D, "D&M Holdings", "Japanese")},
};

// IDs étendus (0x00 + 2 bytes)
// Key = (byte1 << 16) | (byte2 << 8) | byte3
const std::map<uint32_t, ManufacturerInfo> ManufacturerDatabase::extendedIds_ = {
    // American Group (0x00 0x00 0xXX)
    {0x000001, ManufacturerInfo(0x00, 0x00, 0x01, "Time Warner Interactive", "American")},
    {0x000002, ManufacturerInfo(0x00, 0x00, 0x02, "Advanced Gravis Computer Tech", "American")},
    {0x000003, ManufacturerInfo(0x00, 0x00, 0x03, "Media Vision", "American")},
    {0x000004, ManufacturerInfo(0x00, 0x00, 0x04, "Dornes Research Group", "American")},
    {0x000005, ManufacturerInfo(0x00, 0x00, 0x05, "K-Muse", "American")},
    {0x000006, ManufacturerInfo(0x00, 0x00, 0x06, "Stypher", "American")},
    {0x000007, ManufacturerInfo(0x00, 0x00, 0x07, "Digital Music Corp.", "American")},
    {0x000008, ManufacturerInfo(0x00, 0x00, 0x08, "IOTA Systems", "American")},
    {0x000009, ManufacturerInfo(0x00, 0x00, 0x09, "New England Digital", "American")},
    {0x00000A, ManufacturerInfo(0x00, 0x00, 0x0A, "Artisyn", "American")},
    {0x00000B, ManufacturerInfo(0x00, 0x00, 0x0B, "IVL Technologies", "American")},
    {0x00000C, ManufacturerInfo(0x00, 0x00, 0x0C, "Southern Music Systems", "American")},
    {0x00000D, ManufacturerInfo(0x00, 0x00, 0x0D, "Lake Butler Sound Company", "American")},
    {0x00000E, ManufacturerInfo(0x00, 0x00, 0x0E, "Alesis", "American")},
    {0x00000F, ManufacturerInfo(0x00, 0x00, 0x0F, "Sound Creation", "American")},
    {0x000010, ManufacturerInfo(0x00, 0x00, 0x10, "DOD Electronics", "American")},
    {0x000011, ManufacturerInfo(0x00, 0x00, 0x11, "Studer-Editech", "American")},
    {0x000012, ManufacturerInfo(0x00, 0x00, 0x12, "Perfect Fretworks", "American")},
    {0x000013, ManufacturerInfo(0x00, 0x00, 0x13, "KAT", "American")},
    {0x000014, ManufacturerInfo(0x00, 0x00, 0x14, "Opcode", "American")},
    {0x000015, ManufacturerInfo(0x00, 0x00, 0x15, "Rane Corporation", "American")},
    {0x000016, ManufacturerInfo(0x00, 0x00, 0x16, "Spatial Sound", "American")},
    {0x000017, ManufacturerInfo(0x00, 0x00, 0x17, "KMX", "American")},
    {0x000018, ManufacturerInfo(0x00, 0x00, 0x18, "Allen & Heath Brenell", "American")},
    {0x000019, ManufacturerInfo(0x00, 0x00, 0x19, "Peavey Electronics", "American")},
    {0x00001A, ManufacturerInfo(0x00, 0x00, 0x1A, "360 Systems", "American")},
    {0x00001B, ManufacturerInfo(0x00, 0x00, 0x1B, "Spectrum Design and Development", "American")},
    {0x00001C, ManufacturerInfo(0x00, 0x00, 0x1C, "Marquis Music", "American")},
    {0x00001D, ManufacturerInfo(0x00, 0x00, 0x1D, "Zeta Systems", "American")},
    {0x00001E, ManufacturerInfo(0x00, 0x00, 0x1E, "Axxes", "American")},
    {0x00001F, ManufacturerInfo(0x00, 0x00, 0x1F, "Orban", "American")},
    
    // European Group (0x00 0x20 0xXX)
    {0x002001, ManufacturerInfo(0x00, 0x20, 0x01, "KTI", "European")},
    {0x002002, ManufacturerInfo(0x00, 0x20, 0x02, "Breakaway Technologies", "European")},
    {0x002003, ManufacturerInfo(0x00, 0x20, 0x03, "CAE", "European")},
    {0x002004, ManufacturerInfo(0x00, 0x20, 0x04, "Rocktron Corporation", "European")},
    {0x002005, ManufacturerInfo(0x00, 0x20, 0x05, "PianoDisc", "European")},
    {0x002006, ManufacturerInfo(0x00, 0x20, 0x06, "Cannon Research Group", "European")},
    {0x002007, ManufacturerInfo(0x00, 0x20, 0x07, "Rogers Instrument Corporation", "European")},
    {0x002008, ManufacturerInfo(0x00, 0x20, 0x08, "Blue Sky Logic", "European")},
    {0x002009, ManufacturerInfo(0x00, 0x20, 0x09, "Encore Electronics", "European")},
    {0x00200A, ManufacturerInfo(0x00, 0x20, 0x0A, "Uptown", "European")},
    {0x00200B, ManufacturerInfo(0x00, 0x20, 0x0B, "Voce", "European")},
    {0x00200C, ManufacturerInfo(0x00, 0x20, 0x0C, "CTI Audio", "European")},
    {0x00200D, ManufacturerInfo(0x00, 0x20, 0x0D, "S&S Research", "European")},
    {0x00200E, ManufacturerInfo(0x00, 0x20, 0x0E, "Broderbund Software", "European")},
    {0x00200F, ManufacturerInfo(0x00, 0x20, 0x0F, "Allen Organ Co.", "European")},
    {0x002010, ManufacturerInfo(0x00, 0x20, 0x10, "Music Quest", "European")},
    {0x002011, ManufacturerInfo(0x00, 0x20, 0x11, "APHEX", "European")},
    {0x002012, ManufacturerInfo(0x00, 0x20, 0x12, "Gallien Krueger", "European")},
    {0x002013, ManufacturerInfo(0x00, 0x20, 0x13, "IBM", "European")},
    {0x002014, ManufacturerInfo(0x00, 0x20, 0x14, "Mark of the Unicorn", "European")},
    {0x002015, ManufacturerInfo(0x00, 0x20, 0x15, "Hotz Instruments Technologies", "European")},
    {0x002016, ManufacturerInfo(0x00, 0x20, 0x16, "ETA Lighting", "European")},
    {0x002017, ManufacturerInfo(0x00, 0x20, 0x17, "NSI Corporation", "European")},
    {0x002018, ManufacturerInfo(0x00, 0x20, 0x18, "Ad Lib", "European")},
    {0x002019, ManufacturerInfo(0x00, 0x20, 0x19, "Richmond Sound Design", "European")},
    {0x00201A, ManufacturerInfo(0x00, 0x20, 0x1A, "Microsoft", "European")},
    {0x00201B, ManufacturerInfo(0x00, 0x20, 0x1B, "The Software Toolworks", "European")},
    {0x00201C, ManufacturerInfo(0x00, 0x20, 0x1C, "Niche/RJMG", "European")},
    {0x00201D, ManufacturerInfo(0x00, 0x20, 0x1D, "Intone", "European")},
    {0x00201E, ManufacturerInfo(0x00, 0x20, 0x1E, "Advanced Remote Technologies", "European")},
    {0x00201F, ManufacturerInfo(0x00, 0x20, 0x1F, "White Instruments", "European")},
    {0x002020, ManufacturerInfo(0x00, 0x20, 0x20, "Vocaltech", "European")},
    {0x002021, ManufacturerInfo(0x00, 0x20, 0x21, "Tascam", "European")},
    {0x002029, ManufacturerInfo(0x00, 0x20, 0x29, "Focusrite/Novation", "European")},
    {0x00202B, ManufacturerInfo(0x00, 0x20, 0x2B, "TC Electronic", "European")},
    {0x00202F, ManufacturerInfo(0x00, 0x20, 0x2F, "Behringer", "European")},
    {0x002032, ManufacturerInfo(0x00, 0x20, 0x32, "Midas", "European")},
    {0x002033, ManufacturerInfo(0x00, 0x20, 0x33, "Klark Teknik", "European")},
    
    // Japanese Group (0x00 0x40 0xXX)
    {0x004001, ManufacturerInfo(0x00, 0x40, 0x01, "Crimson Technology", "Japanese")},
    {0x004003, ManufacturerInfo(0x00, 0x40, 0x03, "Akai Professional", "Japanese")},
    {0x004004, ManufacturerInfo(0x00, 0x40, 0x04, "Stanton", "Japanese")},
    {0x004005, ManufacturerInfo(0x00, 0x40, 0x05, "Livid Instruments", "Japanese")},
    {0x004006, ManufacturerInfo(0x00, 0x40, 0x06, "Native Instruments", "Japanese")},
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER ManufacturerDatabase.h
// ============================================================================
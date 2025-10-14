// ============================================================================
// Fichier: src/midi/sysex/SysExMessage.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Structure de base pour représenter un message System Exclusive (SysEx).
//   Contient les données brutes et les informations parsées.
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include "UniversalSysEx.h"
#include "DeviceIdentity.h"

namespace midiMind {

/**
 * @enum SysExType
 * @brief Type de message SysEx
 */
enum class SysExType {
    UNKNOWN,                    ///< Type inconnu
    UNIVERSAL_REALTIME,         ///< Universal Real Time
    UNIVERSAL_NON_REALTIME,     ///< Universal Non-Real Time
    MANUFACTURER_SPECIFIC,      ///< Spécifique au fabricant
    IDENTITY_REQUEST,           ///< Demande d'identité
    IDENTITY_REPLY,             ///< Réponse d'identité
    GENERAL_MIDI,               ///< General MIDI
    DEVICE_CONTROL,             ///< Contrôle de device
    TUNING_STANDARD,            ///< MIDI Tuning Standard
    FILE_DUMP,                  ///< File dump
    SAMPLE_DUMP                 ///< Sample dump
};

/**
 * @class SysExMessage
 * @brief Message System Exclusive
 * 
 * @details
 * Représente un message SysEx complet avec ses données brutes
 * et les informations parsées.
 * 
 * Format général:
 * F0 <manufacturer/universal> <data...> F7
 * 
 * Thread-safety: Non thread-safe (à utiliser dans un contexte synchronisé)
 */
class SysExMessage {
public:
    // ========================================================================
    // CONSTRUCTION
    // ========================================================================
    
    /**
     * @brief Constructeur par défaut
     */
    SysExMessage()
        : type_(SysExType::UNKNOWN)
        , valid_(false)
        , timestamp_(0) {}
    
    /**
     * @brief Constructeur avec données brutes
     * 
     * @param data Données SysEx complètes (incluant F0 et F7)
     */
    explicit SysExMessage(const std::vector<uint8_t>& data)
        : rawData_(data)
        , type_(SysExType::UNKNOWN)
        , valid_(false)
        , timestamp_(getCurrentTimestamp()) {
        
        validate();
    }
    
    /**
     * @brief Constructeur avec données brutes (pointeur + taille)
     */
    SysExMessage(const uint8_t* data, size_t size)
        : rawData_(data, data + size)
        , type_(SysExType::UNKNOWN)
        , valid_(false)
        , timestamp_(getCurrentTimestamp()) {
        
        validate();
    }
    
    // ========================================================================
    // VALIDATION
    // ========================================================================
    
    /**
     * @brief Vérifie si le message est valide
     */
    bool isValid() const {
        return valid_;
    }
    
    /**
     * @brief Valide la structure du message
     */
    void validate() {
        if (rawData_.size() < 3) {
            valid_ = false;
            return;
        }
        
        // Vérifier F0 au début et F7 à la fin
        if (rawData_.front() != SysEx::SOX || rawData_.back() != SysEx::EOX) {
            valid_ = false;
            return;
        }
        
        valid_ = true;
        
        // Déterminer le type
        if (rawData_.size() >= 2) {
            uint8_t id = rawData_[1];
            
            if (id == SysEx::UNIVERSAL_REALTIME) {
                type_ = SysExType::UNIVERSAL_REALTIME;
            } else if (id == SysEx::UNIVERSAL_NON_REALTIME) {
                type_ = SysExType::UNIVERSAL_NON_REALTIME;
            } else {
                type_ = SysExType::MANUFACTURER_SPECIFIC;
            }
        }
    }
    
    // ========================================================================
    // ACCESSEURS
    // ========================================================================
    
    /**
     * @brief Récupère les données brutes complètes
     */
    const std::vector<uint8_t>& getRawData() const {
        return rawData_;
    }
    
    /**
     * @brief Récupère les données (sans F0 et F7)
     */
    std::vector<uint8_t> getData() const {
        if (rawData_.size() < 3) {
            return {};
        }
        
        return std::vector<uint8_t>(rawData_.begin() + 1, rawData_.end() - 1);
    }
    
    /**
     * @brief Récupère le type de message
     */
    SysExType getType() const {
        return type_;
    }
    
    /**
     * @brief Récupère la taille totale (incluant F0 et F7)
     */
    size_t getSize() const {
        return rawData_.size();
    }
    
    /**
     * @brief Récupère le timestamp
     */
    uint64_t getTimestamp() const {
        return timestamp_;
    }
    
    /**
     * @brief Définit le timestamp
     */
    void setTimestamp(uint64_t timestamp) {
        timestamp_ = timestamp;
    }
    
    // ========================================================================
    // INFORMATIONS
    // ========================================================================
    
    /**
     * @brief Vérifie si c'est un Universal SysEx
     */
    bool isUniversal() const {
        return type_ == SysExType::UNIVERSAL_REALTIME || 
               type_ == SysExType::UNIVERSAL_NON_REALTIME;
    }
    
    /**
     * @brief Vérifie si c'est un message spécifique au fabricant
     */
    bool isManufacturerSpecific() const {
        return type_ == SysExType::MANUFACTURER_SPECIFIC;
    }
    
    /**
     * @brief Récupère l'ID du fabricant (1er byte après F0)
     */
    uint8_t getManufacturerId() const {
        if (rawData_.size() < 2) {
            return 0;
        }
        return rawData_[1];
    }
    
    /**
     * @brief Récupère le device ID (pour Universal SysEx)
     */
    uint8_t getDeviceId() const {
        if (!isUniversal() || rawData_.size() < 3) {
            return 0;
        }
        return rawData_[2];
    }
    
    /**
     * @brief Récupère le Sub-ID #1 (pour Universal SysEx)
     */
    uint8_t getSubId1() const {
        if (!isUniversal() || rawData_.size() < 4) {
            return 0;
        }
        return rawData_[3];
    }
    
    /**
     * @brief Récupère le Sub-ID #2 (pour Universal SysEx)
     */
    uint8_t getSubId2() const {
        if (!isUniversal() || rawData_.size() < 5) {
            return 0;
        }
        return rawData_[4];
    }
    
    // ========================================================================
    // CONVERSION
    // ========================================================================
    
    /**
     * @brief Convertit en string hexadécimal
     */
    std::string toHexString() const {
        std::string result;
        
        for (size_t i = 0; i < rawData_.size(); ++i) {
            char buf[8];
            snprintf(buf, sizeof(buf), "%02X", rawData_[i]);
            result += buf;
            
            if (i < rawData_.size() - 1) {
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
        j["valid"] = valid_;
        j["type"] = static_cast<int>(type_);
        j["size"] = rawData_.size();
        j["hex"] = toHexString();
        j["timestamp"] = timestamp_;
        
        if (valid_) {
            j["manufacturer_id"] = getManufacturerId();
            
            if (isUniversal()) {
                j["device_id"] = getDeviceId();
                j["sub_id_1"] = getSubId1();
                j["sub_id_2"] = getSubId2();
            }
        }
        
        return j;
    }
    
    /**
     * @brief Convertit en bytes pour envoi
     */
    std::vector<uint8_t> toBytes() const {
        return rawData_;
    }

private:
    /**
     * @brief Récupère le timestamp actuel (ms)
     */
    static uint64_t getCurrentTimestamp() {
        auto now = std::chrono::steady_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    }
    
    /// Données brutes du message (incluant F0 et F7)
    std::vector<uint8_t> rawData_;
    
    /// Type de message
    SysExType type_;
    
    /// Validité du message
    bool valid_;
    
    /// Timestamp de création/réception (ms)
    uint64_t timestamp_;
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER SysExMessage.h
// ============================================================================
// ============================================================================
// Fichier: backend/src/midi/devices/DeviceInfo.h
// Version: 1.0.0
// Date: 2025-10-16
// ============================================================================
// Description:
//   Structure DeviceInfo pour l'identification des périphériques MIDI.
//   Définit les types et métadonnées communs à tous les devices.
//
// Utilisation:
//   Structure partagée entre MidiDeviceManager, DevicePlugin, et les
//   implémentations concrètes de devices (USB, BLE, Network, Virtual).
//
// Auteur: MidiMind Team
// ============================================================================

#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include "../MidiMessage.h"

namespace midiMind {

using json = nlohmann::json;

// ============================================================================
// ÉNUMÉRATIONS - Types de périphériques
// ============================================================================

/**
 * @enum DeviceType
 * @brief Type de périphérique MIDI
 * 
 * @note Défini ici et réutilisé par MidiDevice.h
 */
enum class DeviceType {
    USB,            ///< Périphérique USB (ALSA)
    WIFI,           ///< Périphérique WiFi/Network (RTP-MIDI)
    BLUETOOTH,      ///< Périphérique Bluetooth Low Energy
    VIRTUAL,        ///< Port MIDI virtuel
    NETWORK,        ///< Alias pour WIFI (compatibilité)
    UNKNOWN         ///< Type inconnu
};

/**
 * @enum DeviceDirection
 * @brief Direction du flux MIDI
 */
enum class DeviceDirection {
    INPUT,          ///< Entrée uniquement (receive)
    OUTPUT,         ///< Sortie uniquement (send)
    BIDIRECTIONAL   ///< Entrée et sortie
};

/**
 * @enum DeviceStatus
 * @brief État de connexion du périphérique
 */
enum class DeviceStatus {
    DISCONNECTED,   ///< Déconnecté
    CONNECTING,     ///< Connexion en cours
    CONNECTED,      ///< Connecté et prêt
    ERROR           ///< Erreur de connexion
};

// ============================================================================
// STRUCTURE DeviceInfo
// ============================================================================

/**
 * @struct DeviceInfo
 * @brief Informations complètes sur un périphérique MIDI
 * 
 * @details
 * Structure unifiée pour représenter tous les types de devices MIDI.
 * Utilisée par MidiDeviceManager pour la découverte et la gestion.
 * 
 * Exemples d'IDs:
 * - USB: "usb_001", "usb_128_0"
 * - Network: "wifi_192.168.1.42", "rtpmidi_studio"
 * - Bluetooth: "ble_AA:BB:CC:DD:EE:FF"
 * - Virtual: "virtual_001"
 * 
 * @note Thread-safe pour lecture, synchroniser les modifications
 */
struct DeviceInfo {
    // ========================================================================
    // IDENTIFICATION
    // ========================================================================
    
    std::string id;              ///< ID unique du device
    std::string name;            ///< Nom lisible (ex: "Yamaha PSR-E453")
    DeviceType type;             ///< Type de périphérique
    DeviceDirection direction;   ///< Direction des flux MIDI
    
    // ========================================================================
    // ÉTAT
    // ========================================================================
    
    bool connected;              ///< Est connecté actuellement
    DeviceStatus status;         ///< État de connexion détaillé
    
    // ========================================================================
    // CAPACITÉS
    // ========================================================================
    
    bool hasInput;               ///< Supporte réception MIDI
    bool hasOutput;              ///< Supporte envoi MIDI
    bool supportsSysEx;          ///< Supporte SysEx
    
    // ========================================================================
    // MÉTADONNÉES SPÉCIFIQUES AU TYPE
    // ========================================================================
    
    /**
     * Données spécifiques selon le type de device:
     * 
     * USB:
     *   - "alsa_client": int (client ALSA)
     *   - "alsa_port": int (port ALSA)
     *   - "manufacturer": string
     *   - "model": string
     * 
     * Network/WiFi:
     *   - "address": string (IP)
     *   - "port": int (port RTP-MIDI, défaut 5004)
     *   - "protocol": string ("rtpmidi", "udp")
     * 
     * Bluetooth:
     *   - "address": string (MAC address)
     *   - "rssi": int (signal strength)
     *   - "service_uuid": string
     * 
     * Virtual:
     *   - "alsa_client": int
     *   - "alsa_port": int
     */
    json metadata;
    
    // ========================================================================
    // INFORMATIONS FABRICANT (OPTIONNEL)
    // ========================================================================
    
    std::string manufacturer;    ///< Fabricant (ex: "Yamaha")
    std::string model;           ///< Modèle (ex: "PSR-E453")
    std::string version;         ///< Version firmware
    std::string port;            ///< Port système (ALSA, etc.)
    
    // ========================================================================
    // CONSTRUCTEURS
    // ========================================================================
    
    /**
     * @brief Constructeur par défaut
     */
    DeviceInfo()
        : type(DeviceType::UNKNOWN)
        , direction(DeviceDirection::BIDIRECTIONAL)
        , connected(false)
        , status(DeviceStatus::DISCONNECTED)
        , hasInput(false)
        , hasOutput(false)
        , supportsSysEx(false)
        , metadata(json::object()) {}
    
    /**
     * @brief Constructeur avec paramètres essentiels
     */
    DeviceInfo(const std::string& id_, 
               const std::string& name_,
               DeviceType type_)
        : id(id_)
        , name(name_)
        , type(type_)
        , direction(DeviceDirection::BIDIRECTIONAL)
        , connected(false)
        , status(DeviceStatus::DISCONNECTED)
        , hasInput(true)
        , hasOutput(true)
        , supportsSysEx(false)
        , metadata(json::object()) {}
    
    // ========================================================================
    // MÉTHODES UTILITAIRES
    // ========================================================================
    
    /**
     * @brief Vérifie si le device est valide
     */
    bool isValid() const {
        return !id.empty() && !name.empty() && type != DeviceType::UNKNOWN;
    }
    
    /**
     * @brief Convertit le type en string
     */
    std::string typeToString() const {
        switch (type) {
            case DeviceType::USB: return "USB";
            case DeviceType::WIFI: return "WiFi";
            case DeviceType::NETWORK: return "Network";
            case DeviceType::BLUETOOTH: return "Bluetooth";
            case DeviceType::VIRTUAL: return "Virtual";
            default: return "Unknown";
        }
    }
    
    /**
     * @brief Convertit en JSON
     */
    json toJson() const {
        json j;
        j["id"] = id;
        j["name"] = name;
        j["type"] = typeToString();
        j["direction"] = directionToString();
        j["connected"] = connected;
        j["status"] = statusToString();
        
        j["capabilities"] = {
            {"input", hasInput},
            {"output", hasOutput},
            {"sysex", supportsSysEx}
        };
        
        if (!manufacturer.empty()) j["manufacturer"] = manufacturer;
        if (!model.empty()) j["model"] = model;
        if (!version.empty()) j["version"] = version;
        if (!port.empty()) j["port"] = port;
        
        if (!metadata.empty()) {
            j["metadata"] = metadata;
        }
        
        return j;
    }
    
    /**
     * @brief Crée depuis JSON
     */
    static DeviceInfo fromJson(const json& j) {
        DeviceInfo info;
        
        info.id = j.value("id", "");
        info.name = j.value("name", "");
        info.type = stringToType(j.value("type", "unknown"));
        info.direction = stringToDirection(j.value("direction", "bidirectional"));
        info.connected = j.value("connected", false);
        info.status = stringToStatus(j.value("status", "disconnected"));
        
        if (j.contains("capabilities")) {
            auto cap = j["capabilities"];
            info.hasInput = cap.value("input", false);
            info.hasOutput = cap.value("output", false);
            info.supportsSysEx = cap.value("sysex", false);
        }
        
        info.manufacturer = j.value("manufacturer", "");
        info.model = j.value("model", "");
        info.version = j.value("version", "");
        info.port = j.value("port", "");
        
        if (j.contains("metadata")) {
            info.metadata = j["metadata"];
        }
        
        return info;
    }
    
private:
    /**
     * @brief Convertit direction en string
     */
    std::string directionToString() const {
        switch (direction) {
            case DeviceDirection::INPUT: return "input";
            case DeviceDirection::OUTPUT: return "output";
            case DeviceDirection::BIDIRECTIONAL: return "bidirectional";
            default: return "unknown";
        }
    }
    
    /**
     * @brief Convertit status en string
     */
    std::string statusToString() const {
        switch (status) {
            case DeviceStatus::DISCONNECTED: return "disconnected";
            case DeviceStatus::CONNECTING: return "connecting";
            case DeviceStatus::CONNECTED: return "connected";
            case DeviceStatus::ERROR: return "error";
            default: return "unknown";
        }
    }
    
    /**
     * @brief Convertit string en DeviceType
     */
    static DeviceType stringToType(const std::string& str) {
        if (str == "USB" || str == "usb") return DeviceType::USB;
        if (str == "WiFi" || str == "wifi") return DeviceType::WIFI;
        if (str == "Network" || str == "network") return DeviceType::NETWORK;
        if (str == "Bluetooth" || str == "bluetooth") return DeviceType::BLUETOOTH;
        if (str == "Virtual" || str == "virtual") return DeviceType::VIRTUAL;
        return DeviceType::UNKNOWN;
    }
    
    /**
     * @brief Convertit string en DeviceDirection
     */
    static DeviceDirection stringToDirection(const std::string& str) {
        if (str == "input") return DeviceDirection::INPUT;
        if (str == "output") return DeviceDirection::OUTPUT;
        if (str == "bidirectional") return DeviceDirection::BIDIRECTIONAL;
        return DeviceDirection::BIDIRECTIONAL;
    }
    
    /**
     * @brief Convertit string en DeviceStatus
     */
    static DeviceStatus stringToStatus(const std::string& str) {
        if (str == "disconnected") return DeviceStatus::DISCONNECTED;
        if (str == "connecting") return DeviceStatus::CONNECTING;
        if (str == "connected") return DeviceStatus::CONNECTED;
        if (str == "error") return DeviceStatus::ERROR;
        return DeviceStatus::DISCONNECTED;
    }
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER DeviceInfo.h v1.0.0
// ============================================================================

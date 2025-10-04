// ============================================================================
// Fichier: src/network/discovery/ServiceInfo.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Structure contenant les informations d'un service découvert via mDNS.
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

/**
 * @struct ServiceInfo
 * @brief Informations sur un service réseau découvert
 */
struct ServiceInfo {
    std::string name;           ///< Nom du service
    std::string type;           ///< Type de service (ex: "_apple-midi._udp")
    std::string domain;         ///< Domaine (ex: "local.")
    std::string hostname;       ///< Nom d'hôte
    std::string address;        ///< Adresse IP
    uint16_t port;              ///< Port
    std::vector<std::pair<std::string, std::string>> txtRecords; ///< Enregistrements TXT
    uint64_t lastSeen;          ///< Timestamp dernière découverte (ms)
    
    /**
     * @brief Constructeur par défaut
     */
    ServiceInfo()
        : port(0)
        , lastSeen(0) {}
    
    /**
     * @brief Convertit en JSON
     */
    json toJson() const {
        json j;
        j["name"] = name;
        j["type"] = type;
        j["domain"] = domain;
        j["hostname"] = hostname;
        j["address"] = address;
        j["port"] = port;
        
        json txtJson = json::array();
        for (const auto& [key, value] : txtRecords) {
            txtJson.push_back({{"key", key}, {"value", value}});
        }
        j["txt_records"] = txtJson;
        
        j["last_seen"] = lastSeen;
        
        return j;
    }
    
    /**
     * @brief Récupère une valeur TXT
     */
    std::string getTxtValue(const std::string& key) const {
        for (const auto& [k, v] : txtRecords) {
            if (k == key) {
                return v;
            }
        }
        return "";
    }
    
    /**
     * @brief Vérifie si le service est valide
     */
    bool isValid() const {
        return !name.empty() && !address.empty() && port > 0;
    }
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER ServiceInfo.h
// ============================================================================
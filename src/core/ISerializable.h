// ============================================================================
// Fichier: src/core/ISerializable.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Interface pour la sérialisation JSON.
//   Permet aux objets d'être convertis en JSON et vice-versa.
//
// Pattern: Serialization Pattern
//
// Auteur: MidiMind Team
// Date: 2025-10-03
// Version: 3.0.0
// ============================================================================

#pragma once

#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

namespace midiMind {

/**
 * @class ISerializable
 * @brief Interface pour objets sérialisables
 * 
 * @details
 * Interface de base pour tous les objets qui peuvent être sérialisés
 * en JSON et désérialisés depuis JSON.
 * 
 * Utilisé pour:
 * - Sauvegarde/chargement configuration
 * - Export/import de données
 * - Communication réseau (API REST)
 * - Persistence en base de données
 * 
 * @example Utilisation
 * ```cpp
 * class MyClass : public ISerializable {
 * public:
 *     std::string name;
 *     int value;
 *     
 *     json toJson() const override {
 *         json j;
 *         j["name"] = name;
 *         j["value"] = value;
 *         return j;
 *     }
 *     
 *     void fromJson(const json& j) override {
 *         name = j.value("name", "");
 *         value = j.value("value", 0);
 *     }
 * };
 * ```
 */
class ISerializable {
public:
    virtual ~ISerializable() = default;
    
    /**
     * @brief Convertit l'objet en JSON
     * 
     * @return json Représentation JSON de l'objet
     * 
     * @note Doit être implémenté par les classes dérivées
     */
    virtual json toJson() const = 0;
    
    /**
     * @brief Initialise l'objet depuis JSON
     * 
     * @param j Objet JSON source
     * 
     * @note Doit être implémenté par les classes dérivées
     * @note Doit gérer les valeurs manquantes avec des defaults
     */
    virtual void fromJson(const json& j) = 0;
    
    /**
     * @brief Convertit en string JSON formaté
     * 
     * @param indent Indentation (défaut: 2)
     * @return std::string JSON formaté
     */
    virtual std::string toString(int indent = 2) const {
        return toJson().dump(indent);
    }
    
    /**
     * @brief Sauvegarde dans un fichier JSON
     * 
     * @param filepath Chemin du fichier
     * @return true Si succès
     */
    virtual bool saveToFile(const std::string& filepath) const {
        try {
            std::ofstream file(filepath);
            if (!file.is_open()) return false;
            
            file << toString();
            file.close();
            return true;
        } catch (...) {
            return false;
        }
    }
    
    /**
     * @brief Charge depuis un fichier JSON
     * 
     * @param filepath Chemin du fichier
     * @return true Si succès
     */
    virtual bool loadFromFile(const std::string& filepath) {
        try {
            std::ifstream file(filepath);
            if (!file.is_open()) return false;
            
            json j;
            file >> j;
            file.close();
            
            fromJson(j);
            return true;
        } catch (...) {
            return false;
        }
    }
};

/**
 * @class SerializableBase
 * @brief Classe de base avec implémentation par défaut
 * 
 * @details
 * Fournit une implémentation par défaut de ISerializable
 * qui peut être étendue facilement.
 * 
 * @example Utilisation
 * ```cpp
 * class MyClass : public SerializableBase {
 * public:
 *     std::string name;
 *     
 *     json toJson() const override {
 *         json j = SerializableBase::toJson();
 *         j["name"] = name;
 *         return j;
 *     }
 *     
 *     void fromJson(const json& j) override {
 *         SerializableBase::fromJson(j);
 *         name = j.value("name", "");
 *     }
 * };
 * ```
 */
class SerializableBase : public ISerializable {
public:
    virtual ~SerializableBase() = default;
    
    /**
     * @brief Implémentation par défaut (objet vide)
     */
    json toJson() const override {
        return json::object();
    }
    
    /**
     * @brief Implémentation par défaut (ne fait rien)
     */
    void fromJson(const json& j) override {
        // Par défaut, ne rien faire
    }
};

/**
 * @brief Helper pour convertir un vecteur d'objets sérialisables
 * 
 * @tparam T Type d'objet (doit implémenter ISerializable)
 * @param vec Vecteur d'objets
 * @return json Array JSON
 */
template<typename T>
json vectorToJson(const std::vector<T>& vec) {
    json arr = json::array();
    for (const auto& item : vec) {
        arr.push_back(item.toJson());
    }
    return arr;
}

/**
 * @brief Helper pour convertir un JSON array en vecteur
 * 
 * @tparam T Type d'objet (doit implémenter ISerializable)
 * @param j JSON array
 * @return std::vector<T> Vecteur d'objets
 */
template<typename T>
std::vector<T> jsonToVector(const json& j) {
    std::vector<T> vec;
    
    if (!j.is_array()) return vec;
    
    for (const auto& item : j) {
        T obj;
        obj.fromJson(item);
        vec.push_back(obj);
    }
    
    return vec;
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER ISerializable.h
// ============================================================================
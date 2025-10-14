// ============================================================================
// Fichier: src/midi/sysex/SensorsFeedback.h
// Projet: MidiMind v3.0 - Système d'Orchestration MIDI pour Raspberry Pi
// ============================================================================
// Description:
//   Structure pour Capteurs/Feedback Custom SysEx (protocole 0x7D)
//   Bloc 7 - Monitoring temps réel des capteurs
//
// Auteur: MidiMind Team
// Date: 2025-10-06
// Version: 3.0.0
// ============================================================================

#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace midiMind {

/**
 * @enum SensorType
 * @brief Type de capteur
 */
enum class SensorType : uint8_t {
    UNKNOWN = 0,
    PRESSURE = 1,       ///< Capteur de pression
    FLEX = 2,           ///< Capteur de flexion
    DISTANCE = 3,       ///< Capteur de distance
    ACCELEROMETER = 4,  ///< Accéléromètre
    GYROSCOPE = 5,      ///< Gyroscope
    TOUCH = 6,          ///< Capteur tactile/capacitif
    FORCE = 7,          ///< Capteur de force
    TEMPERATURE = 8,    ///< Capteur de température
    LIGHT = 9,          ///< Capteur de lumière
    MAGNETIC = 10       ///< Capteur magnétique
};

/**
 * @struct SensorInfo
 * @brief Information sur un capteur individuel
 */
struct SensorInfo {
    uint8_t sensorId;        ///< ID du capteur (0-15)
    SensorType sensorType;   ///< Type de capteur
    uint8_t currentValue;    ///< Valeur actuelle (0-127)
    uint8_t minValue;        ///< Valeur min calibrée
    uint8_t maxValue;        ///< Valeur max calibrée
    
    /**
     * @brief Constructeur par défaut
     */
    SensorInfo()
        : sensorId(0)
        , sensorType(SensorType::UNKNOWN)
        , currentValue(0)
        , minValue(0)
        , maxValue(127) {}
    
    /**
     * @brief Obtient le nom du type de capteur
     */
    std::string getSensorTypeName() const {
        switch (sensorType) {
            case SensorType::UNKNOWN: return "Unknown";
            case SensorType::PRESSURE: return "Pressure";
            case SensorType::FLEX: return "Flex";
            case SensorType::DISTANCE: return "Distance";
            case SensorType::ACCELEROMETER: return "Accelerometer";
            case SensorType::GYROSCOPE: return "Gyroscope";
            case SensorType::TOUCH: return "Touch";
            case SensorType::FORCE: return "Force";
            case SensorType::TEMPERATURE: return "Temperature";
            case SensorType::LIGHT: return "Light";
            case SensorType::MAGNETIC: return "Magnetic";
            default: return "Unknown";
        }
    }
    
    /**
     * @brief Calcule le pourcentage de la valeur actuelle
     */
    float getValuePercent() const {
        if (maxValue <= minValue) return 0.0f;
        
        float range = maxValue - minValue;
        float value = currentValue - minValue;
        
        return (value / range) * 100.0f;
    }
    
    /**
     * @brief Vérifie si le capteur est calibré
     */
    bool isCalibrated() const {
        return minValue != maxValue;
    }
    
    /**
     * @brief Convertit en JSON
     */
    json toJson() const {
        json j;
        
        j["id"] = sensorId;
        j["type"]["code"] = static_cast<uint8_t>(sensorType);
        j["type"]["name"] = getSensorTypeName();
        j["value"]["current"] = currentValue;
        j["value"]["min"] = minValue;
        j["value"]["max"] = maxValue;
        j["value"]["percent"] = getValuePercent();
        j["calibrated"] = isCalibrated();
        
        return j;
    }
    
    /**
     * @brief Convertit en string descriptif
     */
    std::string toString() const {
        char buf[64];
        snprintf(buf, sizeof(buf), "Sensor %d (%s): %d/%d-%d",
                sensorId, getSensorTypeName().c_str(), 
                currentValue, minValue, maxValue);
        return std::string(buf);
    }
};

/**
 * @struct SensorsFeedback
 * @brief Monitoring des capteurs (Bloc 7)
 * 
 * @details
 * Structure retournée par le Bloc 7 du protocole Custom SysEx.
 * Contient l'état temps réel de tous les capteurs de l'instrument.
 * 
 * Format du message Bloc 7:
 * F0 7D <DeviceID> 07 03
 * <SensorCount>        // Nombre de capteurs (1-16)
 * [Pour chaque capteur:]
 *   <SensorID>         // ID du capteur (0-15)
 *   <SensorType>       // Type (0-10)
 *   <CurrentValue>     // Valeur actuelle (0-127)
 *   <MinValue>         // Min calibré (0-127)
 *   <MaxValue>         // Max calibré (0-127)
 * F7
 */
struct SensorsFeedback {
    std::vector<SensorInfo> sensors;  ///< Liste des capteurs
    
    /**
     * @brief Constructeur par défaut
     */
    SensorsFeedback() = default;
    
    /**
     * @brief Obtient le nombre de capteurs
     */
    size_t count() const {
        return sensors.size();
    }
    
    /**
     * @brief Ajoute un capteur
     */
    void addSensor(const SensorInfo& sensor) {
        sensors.push_back(sensor);
    }
    
    /**
     * @brief Trouve un capteur par son ID
     */
    const SensorInfo* findSensor(uint8_t sensorId) const {
        for (const auto& sensor : sensors) {
            if (sensor.sensorId == sensorId) {
                return &sensor;
            }
        }
        return nullptr;
    }
    
    /**
     * @brief Compte les capteurs par type
     */
    size_t countByType(SensorType type) const {
        size_t count = 0;
        for (const auto& sensor : sensors) {
            if (sensor.sensorType == type) {
                count++;
            }
        }
        return count;
    }
    
    /**
     * @brief Vérifie si tous les capteurs sont calibrés
     */
    bool allCalibrated() const {
        for (const auto& sensor : sensors) {
            if (!sensor.isCalibrated()) {
                return false;
            }
        }
        return true;
    }
    
    /**
     * @brief Convertit en JSON
     */
    json toJson() const {
        json j;
        
        j["count"] = sensors.size();
        j["allCalibrated"] = allCalibrated();
        
        // Liste des capteurs
        json sensorList = json::array();
        for (const auto& sensor : sensors) {
            sensorList.push_back(sensor.toJson());
        }
        j["sensors"] = sensorList;
        
        // Statistiques par type
        json typeStats;
        typeStats["pressure"] = countByType(SensorType::PRESSURE);
        typeStats["flex"] = countByType(SensorType::FLEX);
        typeStats["distance"] = countByType(SensorType::DISTANCE);
        typeStats["accelerometer"] = countByType(SensorType::ACCELEROMETER);
        typeStats["gyroscope"] = countByType(SensorType::GYROSCOPE);
        typeStats["touch"] = countByType(SensorType::TOUCH);
        typeStats["force"] = countByType(SensorType::FORCE);
        j["typeStats"] = typeStats;
        
        return j;
    }
    
    /**
     * @brief Convertit en string descriptif
     */
    std::string toString() const {
        if (sensors.empty()) {
            return "No sensors";
        }
        
        std::string result = std::to_string(sensors.size()) + " sensor(s)";
        
        if (!allCalibrated()) {
            result += " (some not calibrated)";
        }
        
        return result;
    }
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER SensorsFeedback.h
// ============================================================================

// ============================================================================
// Fichier: src/documentation/CommandDocumentation.h
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace midiMind {

/**
 * @class CommandDocumentation
 * @brief Générateur de documentation API au format JSON
 */
class CommandDocumentation {
public:
    /**
     * @brief Génère la documentation complète de l'API
     * @return json Documentation JSON
     */
    static nlohmann::json generateApiDocumentation() {
        nlohmann::json doc;
        
        doc["api_version"] = "3.0.0";
        doc["last_updated"] = "2025-10-04";
        
        // Categories
        doc["categories"] = nlohmann::json::array({
            "system", "device", "router", "player", 
            "library", "processor", "network"
        });
        
        // Commands (exemple - à compléter)
        doc["commands"] = nlohmann::json::array();
        
        // Exemple de commande
        nlohmann::json cmd;
        cmd["name"] = "system.status";
        cmd["category"] = "system";
        cmd["description"] = "Get system status";
        cmd["parameters"] = nlohmann::json::array();
        doc["commands"].push_back(cmd);
        
        return doc;
    }
    
    /**
     * @brief Sauvegarde la documentation dans un fichier
     * @param filepath Chemin du fichier
     */
    static void saveToFile(const std::string& filepath) {
        auto doc = generateApiDocumentation();
        std::ofstream file(filepath);
        file << doc.dump(2);
        file.close();
    }
};

} // namespace midiMind
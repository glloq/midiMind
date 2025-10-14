// ============================================================================
// Fichier: backend/src/loop/LoopManager.h
// Version: 1.0.0
// Date: 2025-10-10
// ============================================================================
// Description:
//   Gestionnaire de boucles (loops) pour le module Loop Recorder.
//   Gère la création, lecture, mise à jour et suppression des loops.
//   Utilise SQLite pour la persistance.
//
// Responsabilités:
//   - CRUD operations sur les loops
//   - Validation des données
//   - Conversion JSON <-> Structure Loop
//   - Gestion de la base de données
//
// Usage:
//   auto loopMgr = LoopManager::instance();
//   json loop = loopMgr.saveLoop(loopData);
//   json loaded = loopMgr.loadLoop(loopId);
// ============================================================================

#pragma once

#include "../core/Logger.h"
#include "../storage/Database.h"
#include <nlohmann/json.hpp>
#include <string>
#include <memory>
#include <mutex>
#include <optional>

using json = nlohmann::json;

namespace midiMind {

/**
 * @brief Structure représentant un Loop
 */
struct Loop {
    std::string id;
    std::string name;
    int64_t duration;          // ms
    int bars;
    int tempo;                 // BPM
    std::string timeSignature; // "4/4", "3/4", etc.
    json layers;               // Array de layers avec événements MIDI
    int64_t createdAt;         // timestamp Unix ms
    int64_t lastModified;      // timestamp Unix ms
    
    /**
     * @brief Convertit le loop en JSON
     */
    json toJson() const {
        return json{
            {"id", id},
            {"name", name},
            {"duration", duration},
            {"bars", bars},
            {"tempo", tempo},
            {"timeSignature", timeSignature},
            {"layers", layers},
            {"createdAt", createdAt},
            {"lastModified", lastModified}
        };
    }
    
    /**
     * @brief Crée un loop depuis JSON
     */
    static Loop fromJson(const json& j) {
        Loop loop;
        loop.id = j.value("id", "");
        loop.name = j.value("name", "Unnamed Loop");
        loop.duration = j.value("duration", 0);
        loop.bars = j.value("bars", 4);
        loop.tempo = j.value("tempo", 120);
        loop.timeSignature = j.value("timeSignature", "4/4");
        loop.layers = j.value("layers", json::array());
        loop.createdAt = j.value("createdAt", 0);
        loop.lastModified = j.value("lastModified", 0);
        return loop;
    }
};

/**
 * @brief Gestionnaire de loops (Singleton)
 */
class LoopManager {
public:
    /**
     * @brief Obtient l'instance singleton
     */
    static LoopManager& instance() {
        static LoopManager instance;
        return instance;
    }
    
    // Supprimer constructeur de copie et opérateur d'affectation
    LoopManager(const LoopManager&) = delete;
    LoopManager& operator=(const LoopManager&) = delete;
    
    /**
     * @brief Initialise le gestionnaire
     * @param database Instance de la base de données
     */
    void initialize(std::shared_ptr<Database> database);
    
    /**
     * @brief Crée la table loops si elle n'existe pas
     */
    void createTableIfNeeded();
    
    /**
     * @brief Sauvegarde un loop (création ou mise à jour)
     * @param loopData Données du loop en JSON
     * @return Loop sauvegardé avec ID généré si nouveau
     * @throws MidiMindException si validation échoue
     */
    json saveLoop(const json& loopData);
    
    /**
     * @brief Charge un loop par ID
     * @param loopId Identifiant unique du loop
     * @return Loop en JSON ou std::nullopt si non trouvé
     */
    std::optional<json> loadLoop(const std::string& loopId);
    
    /**
     * @brief Liste tous les loops
     * @param limit Nombre maximum de résultats (défaut: 50)
     * @param offset Offset pour pagination (défaut: 0)
     * @param sortBy Champ de tri ("name", "lastModified", défaut: "lastModified")
     * @param sortOrder Ordre de tri ("asc" ou "desc", défaut: "desc")
     * @return Array JSON de loops
     */
    json listLoops(int limit = 50, 
                   int offset = 0,
                   const std::string& sortBy = "lastModified",
                   const std::string& sortOrder = "desc");
    
    /**
     * @brief Supprime un loop
     * @param loopId Identifiant du loop à supprimer
     * @return true si supprimé, false si non trouvé
     */
    bool deleteLoop(const std::string& loopId);
    
    /**
     * @brief Recherche des loops par nom
     * @param query Terme de recherche
     * @param limit Nombre maximum de résultats
     * @return Array JSON de loops correspondants
     */
    json searchLoops(const std::string& query, int limit = 20);
    
    /**
     * @brief Compte le nombre total de loops
     * @return Nombre de loops dans la base
     */
    int getTotalCount();
    
    /**
     * @brief Valide les données d'un loop
     * @param loopData Données à valider
     * @return true si valide
     * @throws MidiMindException avec détails si invalide
     */
    bool validateLoop(const json& loopData);
    
private:
    LoopManager() = default;
    ~LoopManager() = default;
    
    std::shared_ptr<Database> database_;
    mutable std::mutex mutex_;
    bool initialized_ = false;
    
    /**
     * @brief Génère un ID unique pour un nouveau loop
     */
    std::string generateLoopId();
    
    /**
     * @brief Vérifie si un loop existe
     */
    bool loopExists(const std::string& loopId);
    
    /**
     * @brief Insère un nouveau loop
     */
    void insertLoop(const Loop& loop);
    
    /**
     * @brief Met à jour un loop existant
     */
    void updateLoop(const Loop& loop);
    
    /**
     * @brief Convertit une ligne SQL en Loop
     */
    Loop rowToLoop(const json& row);
};

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER LoopManager.h
// ============================================================================
